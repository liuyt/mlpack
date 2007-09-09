/**
 * @file series_expansion.cc
 *
 */

#include <values.h>

#include "series_expansion.h"

void SeriesExpansion::ComputeFarFieldCoeffs(const Matrix& data,
					    const Vector& weights,
					    const ArrayList<int>& rows, 
					    int order, 
					    const SeriesExpansionAux& sea) {

  int dim = data.n_rows();
  int total_num_coeffs = sea.get_total_num_coeffs(order);
  Vector tmp;
  int num_rows = rows.size();
  int r, i, j, k, t, tail;
  Vector heads;
  Vector x_r;
  Vector C_k;
  double bw_times_sqrt_two = sqrt(2 * bwsqd_);

  // initialize temporary variables
  tmp.Init(total_num_coeffs);
  heads.Init(dim + 1);
  x_r.Init(dim);

  // If we have more than what we need, return.
  if(order_ >= order) {
    return;
  }
  else {
    order_ = order;
  }
    
  // Repeat for each reference point in this reference node.
  for(r = 0; r < num_rows; r++) {
    
    // get the row number.
    int row_num = rows[r];
    
    // Calculate the coordinate difference between the ref point and the 
    // centroid.
    for(i = 0; i < dim; i++) {
      x_r[i] = (data.get(i, row_num) - center_[i]) / bw_times_sqrt_two;
    }

    // initialize heads
    heads.SetZero();
    heads[dim] = MAXINT;
    
    tmp[0] = 1.0;
    
    for(k = 1, t = 1, tail = 1; k <= order; k++, tail = t) {
      for(i = 0; i < dim; i++) {
	int head = (int) heads[i];
	heads[i] = t;
	
	for(j = head; j < tail; j++, t++) {
	  tmp[t] = tmp[j] * x_r[i];
	}
      }
    }
    
    // Tally up the result in A_k.
    for(i = 0; i < total_num_coeffs; i++) {
      coeffs_[i] += weights[row_num] * tmp[i];
    }
    
  } /* End of looping through each reference point */ 

  // get multiindex factors
  C_k.Alias(sea.get_inv_multiindex_factorials());

  for(r = 1; r < total_num_coeffs; r++) {
    coeffs_[r] = coeffs_[r] * C_k[r];
  }
}

void SeriesExpansion::ComputeLocalCoeffs(const Matrix& data,
					 const Vector& weights,
					 const ArrayList<int>& rows, 
					 int order,
					 const SeriesExpansionAux& sea) {


  if(order > order_) {
    order_ = order;
  }

  int dim = sea.get_dimension();
  int total_num_coeffs = sea.get_total_num_coeffs(order);
  
  // get inverse factorials (precomputed)
  Vector neg_inv_multiindex_factorials;
  neg_inv_multiindex_factorials.Alias
    (sea.get_neg_inv_multiindex_factorials());

  // declare hermite mapping
  Matrix hermite_map;
  hermite_map.Init(dim, order + 1);
  
  // some temporary variables
  Vector arrtmp;
  arrtmp.Init(total_num_coeffs);
  Vector x_r_minus_x_Q;
  x_r_minus_x_Q.Init(dim);

  // sqrt two times bandwidth
  double sqrt_two_bandwidth = sqrt(2 * bwsqd_);

  // for each data point,
  for(index_t r = 0; r < rows.size(); r++) {

    // get the row number
    int row_num = rows[r];

    // calculate x_r - x_Q
    for(index_t d = 0; d < dim; d++) {
      x_r_minus_x_Q[d] = (center_[d] - data.get(d, row_num)) / 
	sqrt_two_bandwidth;
    }
    
    // precompute necessary Hermite polynomials based on coordinate difference
    for(index_t d = 0; d < dim; d++) {

      double coord_div_band = x_r_minus_x_Q[d];
      double d2 = 2 * coord_div_band;
      double facj = exp(-coord_div_band * coord_div_band);
      
      hermite_map.set(d, 0, facj);
      
      if(order > 0) {

	hermite_map.set(d, 1, d2 * facj);
	
	if(order > 1) {
	  for(index_t k = 1; k < order; k++) {
	    int k2 = k * 2;
	    hermite_map.set(d, k + 1, d2 * hermite_map.get(d, k) -
			    k2 * hermite_map.get(d, k - 1));
	  }
	}
      }
    } // end of looping over each dimension
    
    // compute h_{beta}((x_r - x_Q) / sqrt(2h^2))
    for(index_t j = 0; j < total_num_coeffs; j++) {
      ArrayList<int> mapping = sea.get_multiindex(j);
      arrtmp[j] = 1.0;

      for(index_t d = 0; d < dim; d++) {
        arrtmp[j] *= hermite_map.get(d, mapping[d]);
      }
    }

    for(index_t j = 0; j < total_num_coeffs; j++) {
      coeffs_[j] += neg_inv_multiindex_factorials[j] * weights[row_num] * 
	arrtmp[j];
    }
  } // End of looping through each reference point.
}

double SeriesExpansion::EvaluateFarField(Matrix* data, int row_num,
					 Vector* x_q,
					 SeriesExpansionAux *sea) {
  
  // dimension
  int dim = sea->get_dimension();

  // total number of coefficients
  int total_num_coeffs = sea->get_total_num_coeffs(order_);

  // square root times bandwidth
  double sqrt_two_bandwidth = sqrt(2 * bwsqd_);
  
  // the evaluated sum
  double multipole_sum = 0;
  
  // computed derivative map
  Matrix derivative_map;
  derivative_map.Init(dim, order_ + 1);

  // temporary variable
  Vector arrtmp;
  arrtmp.Init(total_num_coeffs);

  // (x_q - x_R) scaled by bandwidth
  Vector x_q_minus_x_R;
  x_q_minus_x_R.Init(dim);

  // compute (x_q - x_R) / (sqrt(2h^2))
  for(index_t d = 0; d < dim; d++) {
    if(x_q == NULL) {
      x_q_minus_x_R[d] = (data->get(d, row_num) - center_[d]) / 
	sqrt_two_bandwidth;
    }
    else {
      x_q_minus_x_R[d] = ((*x_q)[d] - center_[d]) / sqrt_two_bandwidth;
    }
  }

  // compute deriative maps based on coordinate difference.
  for(index_t d = 0; d < dim; d++) {
    double coord_div_band = x_q_minus_x_R[d];
    double d2 = 2 * coord_div_band;
    double facj = exp(-coord_div_band * coord_div_band);

    derivative_map.set(d, 0, facj);

    if(order_ > 0) {
      derivative_map.set(d, 1, d2 * facj);
    
      if(order_ > 1) {
	for(index_t k = 1; k < order_; k++) {
	  int k2 = k * 2;
	  derivative_map.set(d, k + 1, d2 * derivative_map.get(d, k) -
			     k2 * derivative_map.get(d, k - 1));
	}
      }
    }
  }

  // compute h_{\alpha}((x_q - x_R)/sqrt(2h^2))
  for(index_t j = 0; j < total_num_coeffs; j++) {
    ArrayList<int> mapping = sea->get_multiindex(j);
    arrtmp[j] = 1.0;
    
    for(index_t d = 0; d < dim; d++) {
      arrtmp[j] *= derivative_map.get(d, mapping[d]);
    }
  }
  
  // tally up the multipole sum
  for(index_t j = 0; j < total_num_coeffs; j++) {
    multipole_sum += coeffs_[j] * arrtmp[j];
  }

  return multipole_sum;
}

double SeriesExpansion::EvaluateLocalField(Matrix* data, int row_num,
					   Vector* x_q,
					   SeriesExpansionAux *sea) {
  index_t k, t, tail;
  
  // total number of coefficient
  int total_num_coeffs = sea->get_total_num_coeffs(order_);

  // number of dimensions
  int dim = sea->get_dimension();

  // evaluated sum to be returned
  double sum = 0;
  
  // sqrt two bandwidth
  double sqrt_two_bandwidth = sqrt(2 * bwsqd_);

  // temporary variable
  Vector x_Q_to_x_q;
  x_Q_to_x_q.Init(dim);
  Vector tmp;
  tmp.Init(total_num_coeffs);
  ArrayList<int> heads;
  heads.Init(dim + 1);
  
  // compute (x_q - x_Q) / (sqrt(2h^2))
  for(index_t i = 0; i < dim; i++) {
    
    if(data == NULL) {
      x_Q_to_x_q[i] = ((*x_q)[i] - center_[i]) / sqrt_two_bandwidth;
    }
    else {
      x_Q_to_x_q[i] = (data->get(i, row_num) - center_[i]) / 
	sqrt_two_bandwidth;
    }
  }
  
  for(index_t i = 0; i < dim; i++)
    heads[i] = 0;
  heads[dim] = MAXINT;

  tmp[0] = 1.0;

  for(k = 1, t = 1, tail = 1; k <= order_; k++, tail = t) {

    for(index_t i = 0; i < dim; i++) {
      int head = heads[i];
      heads[i] = t;

      for(index_t j = head; j < tail; j++, t++) {
        tmp[t] = tmp[j] * x_Q_to_x_q[i];
      }
    }
  }

  for(index_t i = 0; i < total_num_coeffs; i++) {
    sum += coeffs_[i] * tmp[i];
  }

  return sum;
}

void SeriesExpansion::Init(KernelType kernel_type, 
			   ExpansionType expansion_type, 
			   const Vector& center, int max_total_num_coeffs, 
			   double bwsqd) {

  // copy kernel type, center, and bandwidth squared
  kernel_type_ = kernel_type;
  expansion_type_ = expansion_type;
  center_.Copy(center);
  bwsqd_ = bwsqd;
  order_ = 0;

  // initialize coefficient array
  coeffs_.Init(max_total_num_coeffs);
  coeffs_.SetZero();
}

void SeriesExpansion::PrintDebug(const char *name, FILE *stream) const {
  
  fprintf(stream, "----- SERIESEXPANSION %s ------\n", name);
  fprintf(stream, "Kernel type: %s\n", (kernel_type_ == GAUSSIAN) ?
	  "GAUSSIAN":"EPANECHNIKOV");
  fprintf(stream, "Expansion type: %s\n", (expansion_type_ == FARFIELD) ?
	  "FARFIELD":"LOCAL");
  fprintf(stream, "Center: ");
  
  for (index_t i = 0; i < center_.length(); i++) {
    fprintf(stream, "%g ", center_[i]);
  }
  fprintf(stream, "\n");
  
  for (index_t i = 0; i < coeffs_.length(); i++) {
    fprintf(stream, "%g ", coeffs_[i]);
  }
  fprintf(stream, "\n");
}

void SeriesExpansion::TransFarToFar(const SeriesExpansion &se,
				    const SeriesExpansionAux &sea) {

  double sqrt_two_bandwidth = sqrt(2 * se.get_bwsqd());
  int dim = sea.get_dimension();
  int order = se.get_order();
  int total_num_coeffs = sea.get_total_num_coeffs(order);
  Vector prev_coeffs;
  Vector prev_center;
  const ArrayList < int > *multiindex_mapping = sea.get_multiindex_mapping();
  ArrayList <int> tmp_storage;
  Vector center_diff;
  Vector inv_multiindex_factorials;

  center_diff.Init(dim);

  // retrieve coefficients to be translated and helper mappings
  prev_coeffs.Alias(se.get_coeffs());
  prev_center.Alias(se.get_center());
  tmp_storage.Init(sea.get_dimension());
  inv_multiindex_factorials.Alias(sea.get_inv_multiindex_factorials());

  // no coefficients can be translated
  if(order == 0)
    return;
  
  // the first order (the sum of the weights) stays constant regardless
  // of the location of the center.
  coeffs_.SetZero();
  coeffs_[0] = prev_coeffs[0];
  
  // compute center difference
  for(index_t j = 0; j < dim; j++) {
    center_diff[j] = prev_center[j] - center_[j];
  }

  for(index_t j = 1; j < total_num_coeffs; j++) {
    
    ArrayList <int> gamma_mapping = multiindex_mapping[j];
    
    for(index_t k = 0; k <= j; k++) {
      ArrayList <int> inner_mapping = multiindex_mapping[k];
      int flag = 0;
      double diff1;
      
      // compute gamma minus alpha
      for(index_t l = 0; l < dim; l++) {
	tmp_storage[l] = gamma_mapping[l] - inner_mapping[l];

	if(tmp_storage[l] < 0) {
	  flag = 1;
	  break;
	}
      }
      
      if(flag) {
	continue;
      }
      
      diff1 = 1.0;
      
      for(index_t l = 0; l < dim; l++) {

	diff1 *= pow(center_diff[l] / sqrt_two_bandwidth, tmp_storage[l]);
      }

      coeffs_[j] += prev_coeffs[k] * diff1 * 
	inv_multiindex_factorials[sea.ComputeMultiindexPosition(tmp_storage)];

    } /* end of k-loop */
  } /* end of j-loop */
}

void SeriesExpansion::TransFarToLocal(const SeriesExpansion &se,
				      const SeriesExpansionAux &sea) {

  Vector arrtmp;
  Matrix hermite_map;
  Vector far_center;
  Vector far_coeffs;
  int dimension = sea.get_dimension();
  int far_order = se.get_order();
  int total_num_coeffs = sea.get_total_num_coeffs(far_order);
  int limit;
  double bw_times_sqrt_two = sqrt(2 * bwsqd_);

  // get center and coefficients for far field expansion
  far_center.Alias(se.get_center());
  far_coeffs.Alias(se.get_coeffs());

  // if the order of the far field expansion is greater than the
  // local one we are adding onto, then increase the order.
  if(far_order > order_) {
    order_ = far_order;
  }

  // compute Gaussian derivative
  limit = 2 * order_ + 1;
  hermite_map.Init(dimension, limit);
  arrtmp.Init(total_num_coeffs);
  
  for(index_t j = 0; j < dimension; j++) {
    double coord_div_band = (center_[j] - far_center[j]) / bw_times_sqrt_two;
    double d2 = 2 * coord_div_band;
    double facj = exp(-coord_div_band * coord_div_band);

    hermite_map.set(j, 0, facj);
    
    if(order_ > 0) {
      hermite_map.set(j, 1, d2 * facj);

      for(index_t k = 1; k < limit; k++) {
	int k2 = k * 2;
	hermite_map.set(j, k + 1, d2 * hermite_map.get(j, k) - k2 * 
			hermite_map.get(j, k - 1));
      } // end of k loop
    }
  } // end of j-loop

  for(index_t j = 0; j < total_num_coeffs; j++) {

    ArrayList<int> beta_mapping = sea.get_multiindex(j);
    arrtmp[j] = 0;

    for(index_t k = 0; k < total_num_coeffs; k++) {

      ArrayList<int> alpha_mapping = sea.get_multiindex(k);
      double hermite_factor = 1.0;

      for(index_t d = 0; d < dimension; d++) {
	hermite_factor *= 
	  hermite_map.get(d, beta_mapping[d] + alpha_mapping[d]);
      }
      
      arrtmp[j] += far_coeffs[k] * hermite_factor;

    } // end of k-loop
  } // end of j-loop

  Vector C_k_neg = sea.get_neg_inv_multiindex_factorials();
  for(index_t j = 0; j < total_num_coeffs; j++) {
    coeffs_[j] += arrtmp[j] * C_k_neg[j];
  }
}

void SeriesExpansion::TransLocalToLocal(const SeriesExpansion &se,
					const SeriesExpansionAux &sea) {
  
  // get the center and the order and the total number of coefficients of 
  // the expansion we are translating from. Also get coefficients we
  // are translating
  Vector prev_center;
  prev_center.Alias(se.get_center());
  int prev_order = se.get_order();
  int total_num_coeffs = sea.get_total_num_coeffs(prev_order);
  Vector prev_coeffs;
  prev_coeffs.Alias(se.get_coeffs());

  // dimension
  int dim = sea.get_dimension();

  // temporary variable
  ArrayList<int> tmp_storage;
  tmp_storage.Init(dim);

  // sqrt two times bandwidth
  double sqrt_two_bandwidth = sqrt(2 * bwsqd_);

  // center difference between the old center and the new one
  Vector center_diff;
  center_diff.Init(dim);
  for(index_t d = 0; d < dim; d++) {
    center_diff[d] = (center_[d] - prev_center[d]) / sqrt_two_bandwidth;
  }

  // set to the new order if the order of the expansion we are translating
  // from is higher
  if(prev_order > order_) {
    order_ = prev_order;
  }

  // inverse multiindex factorials
  Vector C_k;
  C_k.Alias(sea.get_inv_multiindex_factorials());
  
  // do the actual translation
  for(index_t j = 0; j < total_num_coeffs; j++) {

    ArrayList<int> alpha_mapping = sea.get_multiindex(j);
    
    for(index_t k = j; k < total_num_coeffs; k++) {

      ArrayList<int> beta_mapping = sea.get_multiindex(k);
      int flag = 0;
      double diff1 = 1.0;

      for(index_t l = 0; l < dim; l++) {
	tmp_storage[l] = beta_mapping[l] - alpha_mapping[l];

	if(tmp_storage[l] < 0) {
	  flag = 1;
	  break;
	}
      } // end of looping over dimension
      
      if(flag)
	continue;

      for(index_t l = 0; l < dim; l++) {
	diff1 *= pow(center_diff[l], tmp_storage[l]);
      }
      coeffs_[j] += prev_coeffs[k] * diff1 *
	sea.get_n_multichoose_k_by_pos(k, j);

    } // end of k loop
  } // end of j loop
}
