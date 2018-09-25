// License: BSD 3 clause

//
// Created by Stéphane GAIFFAS on 12/12/2015.
//

#include "tick/linear_model/model_linreg.h"

template <class T, class K>
T TModelLinReg<T, K>::sdca_dual_min_i(const ulong i, const T dual_i,
                                      const Array<K> &primal_vector,
                                      const T previous_delta_dual_i, T l_l2sq) {
  compute_features_norm_sq();
  T normalized_features_norm = features_norm_sq[i] / (l_l2sq * n_samples);
  if (use_intercept()) {
    normalized_features_norm += 1. / (l_l2sq * n_samples);
  }
  const T primal_dot_features = get_inner_prod(i, primal_vector);
  const T label = get_label(i);
  const T delta_dual =
      -(dual_i + primal_dot_features - label) / (1 + normalized_features_norm);
  return delta_dual;
}

template <class T, class K>
Array<T> TModelLinReg<T, K>::sdca_dual_min_many(const ArrayULong indices,
                                                const Array<T> duals,
                                                const Array<K> &primal_vector,
                                                double l_l2sq) {
  const ulong n_indices = indices.size();

  // TODO: pass these weights by SDCA
  Array<T> p = Array<T>(n_indices);
  Array2d<T> g = Array2d<T>(n_indices, n_indices);
  Array<T> sdca_labels = Array<T>(n_indices);

  Array<T> n_grad = Array<T>(n_indices);
  Array2d<T> n_hess = Array2d<T>(n_indices, n_indices);

  Array<T> new_duals = Array<T>(n_indices);
  Array<T> delta_duals = Array<T>(n_indices);

  ArrayInt ipiv = ArrayInt(n_indices);

  for (ulong i = 0; i < n_indices; ++i) sdca_labels[i] = get_label(indices[i]);

  const double _1_lambda_n = 1 / (l_l2sq * n_samples);
  for (ulong i = 0; i < n_indices; ++i) {
    for (ulong j = 0; j < n_indices; ++j) {
      if (j < i) g(i, j) = g(j, i);
      else if (i == j) g(i, i) = features_norm_sq[indices[i]] * _1_lambda_n;
      else g(i, j) = get_features(indices[i]).dot(get_features(indices[j])) * _1_lambda_n;
      if (use_intercept()) g(i, j) += _1_lambda_n;
    }
  }

  for (ulong i = 0; i < n_indices; ++i) p[i] = get_inner_prod(indices[i], primal_vector);
  for (ulong i = 0; i < n_indices; ++i) delta_duals[i] = 0;


  for (int k = 0; k < 20; ++k) {

    for (ulong i = 0; i < n_indices; ++i) {
      new_duals[i] = duals[i] + delta_duals[i];
    }

    // Use the opposite to have a positive definite matrix
    for (ulong i = 0; i < n_indices; ++i) {
      n_grad[i] = new_duals[i] - sdca_labels[i] + p[i];

      for (ulong j = 0; j < n_indices; ++j) {
        n_grad[i] += delta_duals[j] * g(i, j);
        n_hess(i, j) = g(i, j);
      }

      n_hess(i, i) += 1;
    }

    // it seems faster this way
    if (n_indices <= 30) {
      tick::vector_operations<T>{}.solve_linear_system(
          n_indices, n_hess.data(), n_grad.data(), ipiv.data());
    } else {
      tick::vector_operations<T>{}.solve_symmetric_linear_system(
          n_indices, n_hess.data(), n_grad.data());
    }

    delta_duals.mult_incr(n_grad, -1.);

    bool all_converged = true;
    for (ulong i = 0; i < n_indices; ++i) {
      all_converged &= std::abs(n_grad[i]) < 1e-10;
    }
    if (all_converged) {
      break;
    }
  }

  double mean = 0;
  for (ulong i = 0; i < n_indices; ++i) {
    const double abs_grad_i = std::abs(n_grad[i]);
    mean += abs_grad_i;
  }
  mean /= n_indices;

  if (mean > 1e-4) std::cout << "did not converge with mean=" << mean << std::endl;

  return delta_duals;
};

template <class T, class K>
T TModelLinReg<T, K>::loss_i(const ulong i, const Array<K> &coeffs) {
  // Compute x_i^T \beta + b
  const T z = get_inner_prod(i, coeffs);
  const T d = get_label(i) - z;
  return d * d / 2;
}

template <class T, class K>
T TModelLinReg<T, K>::grad_i_factor(const ulong i, const Array<K> &coeffs) {
  const T z = get_inner_prod(i, coeffs);
  return z - get_label(i);
}

template <class T, class K>
void TModelLinReg<T, K>::compute_lip_consts() {
  if (ready_lip_consts) {
    return;
  } else {
    compute_features_norm_sq();
    lip_consts = Array<T>(n_samples);
    for (ulong i = 0; i < n_samples; ++i) {
      if (fit_intercept) {
        lip_consts[i] = features_norm_sq[i] + 1;
      } else {
        lip_consts[i] = features_norm_sq[i];
      }
    }
  }
}

template class DLL_PUBLIC TModelLinReg<double, double>;
template class DLL_PUBLIC TModelLinReg<float, float>;

template class DLL_PUBLIC TModelLinReg<double, std::atomic<double>>;
template class DLL_PUBLIC TModelLinReg<float, std::atomic<float>>;
