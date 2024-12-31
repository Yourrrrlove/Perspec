#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "perspectivetransform.h"

// #define DEBUG_LOGGING

#ifdef DEBUG_LOGGING
  #define log(msg) printf("DEBUG: %s\n", msg)
#else
  #define log(msg) // No operation
#endif

// Helper function to solve 8x8 linear system using Gaussian elimination
// Returns 1 on success, 0 on failure
int solve_linear_system(double A[8][8], double b[8], double x[8]) {
  const int n = 8;
  const double epsilon = 1e-10;
  int i, j, k;

  // Create augmented matrix [A|b] with extra safety margin
  double aug[8][10];  // One extra column for safety

  // Initialize augmented matrix
  for(i = 0; i < n; i++) {
    for(j = 0; j < n; j++) {
      aug[i][j] = A[i][j];
    }
    aug[i][n] = b[i];
  }

  // Gaussian elimination with partial pivoting
  for(i = 0; i < n; i++) {
    // Find pivot
    int max_row = i;
    double max_val = fabs(aug[i][i]);

    for(k = i + 1; k < n; k++) {
      if(fabs(aug[k][i]) > max_val) {
        max_val = fabs(aug[k][i]);
        max_row = k;
      }
    }

    // Check for singularity
    if(max_val < epsilon) {
      log("Warning: Matrix is nearly singular\n");
      return 0;
    }

    // Swap maximum row with current row
    if(max_row != i) {
      for(j = 0; j <= n; j++) {
        double temp = aug[i][j];
        aug[i][j] = aug[max_row][j];
        aug[max_row][j] = temp;
      }
    }

    // Eliminate column i
    for(j = i + 1; j < n; j++) {
      double factor = aug[j][i] / aug[i][i];
      for(k = i; k <= n; k++) {
        aug[j][k] -= factor * aug[i][k];
      }
    }
  }

  // Back substitution
  for(i = n - 1; i >= 0; i--) {
    if(fabs(aug[i][i]) < epsilon) {
      log("Warning: Zero pivot encountered\n");
      return 0;
    }

    x[i] = aug[i][n];
    for(j = i + 1; j < n; j++) {
      x[i] -= aug[i][j] * x[j];
    }
    x[i] /= aug[i][i];

    // Check for invalid results
    if(isnan(x[i]) || isinf(x[i])) {
      log("Warning: Invalid result detected\n");
      return 0;
    }
  }

  return 1;
}

// Normalize points to improve numerical stability
static void normalize_points(Point2D points[4], double *scale, double *tx, double *ty) {
  double mean_x = 0, mean_y = 0;
  double sum_dist = 0;
  int i;

  // Calculate centroid
  for(i = 0; i < 4; i++) {
    mean_x += points[i].x;
    mean_y += points[i].y;
  }
  mean_x /= 4;
  mean_y /= 4;

  // Calculate average distance from centroid
  for(i = 0; i < 4; i++) {
    double dx = points[i].x - mean_x;
    double dy = points[i].y - mean_y;
    sum_dist += sqrt(dx*dx + dy*dy);
  }
  double avg_dist = sum_dist / 4;

  // Scale to make average distance from centroid = sqrt(2)
  *scale = (avg_dist > 1e-10) ? sqrt(2.0) / avg_dist : 1.0;
  *tx = -mean_x;
  *ty = -mean_y;

  // Apply normalization transform
  for(i = 0; i < 4; i++) {
    points[i].x = (points[i].x + *tx) * *scale;
    points[i].y = (points[i].y + *ty) * *scale;
  }
}

Matrix3x3 *calculate_perspective_transform(Corners *src_corners, Corners *dst_corners) {
  // Initialize matrices with zeros
  double A[8][8] = {{0}};
  double b[8] = {0};
  double x[8] = {0};

  // Identity matrix as fallback
  static Matrix3x3 identity = {
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0
  };

  if (!src_corners || !dst_corners) {
    log("Error: NULL pointer passed to calculate_perspective_transform\n");
    return &identity;
  }

  #ifdef DEBUG_LOGGING
    printf("[C] Calculating perspective transform:\n");
    printf("src_corners:\ntl(%f, %f)\ntr(%f, %f)\nbr(%f, %f)\nbl(%f, %f)\n\n",
      src_corners->tl_x, src_corners->tl_y,
      src_corners->tr_x, src_corners->tr_y,
      src_corners->br_x, src_corners->br_y,
      src_corners->bl_x, src_corners->bl_y
    );
    printf("dst_corners:\ntl(%f, %f)\ntr(%f, %f)\nbr(%f, %f)\nbl(%f, %f)\n\n",
      dst_corners->tl_x, dst_corners->tl_y,
      dst_corners->tr_x, dst_corners->tr_y,
      dst_corners->br_x, dst_corners->br_y,
      dst_corners->bl_x, dst_corners->bl_y
    );
  #endif

  // Validate input coordinates
  if (
    isnan(src_corners->tl_x) || isnan(src_corners->tl_y) ||
    isnan(src_corners->tr_x) || isnan(src_corners->tr_y) ||
    isnan(src_corners->br_x) || isnan(src_corners->br_y) ||
    isnan(src_corners->bl_x) || isnan(src_corners->bl_y) ||
    isnan(dst_corners->tl_x) || isnan(dst_corners->tl_y) ||
    isnan(dst_corners->tr_x) || isnan(dst_corners->tr_y) ||
    isnan(dst_corners->br_x) || isnan(dst_corners->br_y) ||
    isnan(dst_corners->bl_x) || isnan(dst_corners->bl_y)
  ) {
    log("Error: Invalid coordinates (NaN) detected\n");
    return &identity;
  }

  // Set up the system of equations
  for(int i = 0; i < 4; i++) {
    double srcX = 0.0, srcY = 0.0, dstX = 0.0, dstY = 0.0;

    // Safely extract coordinates
    switch(i) {
      case 0: // Top-left
        srcX = src_corners->tl_x; srcY = src_corners->tl_y;
        dstX = dst_corners->tl_x; dstY = dst_corners->tl_y;
        break;
      case 1: // Top-right
        srcX = src_corners->tr_x; srcY = src_corners->tr_y;
        dstX = dst_corners->tr_x; dstY = dst_corners->tr_y;
        break;
      case 2: // Bottom-right
        srcX = src_corners->br_x; srcY = src_corners->br_y;
        dstX = dst_corners->br_x; dstY = dst_corners->br_y;
        break;
      case 3: // Bottom-left
        srcX = src_corners->bl_x; srcY = src_corners->bl_y;
        dstX = dst_corners->bl_x; dstY = dst_corners->bl_y;
        break;
    }

    // Validate extracted coordinates
    if (isinf(srcX) || isinf(srcY) || isinf(dstX) || isinf(dstY)) {
      log("Error: Invalid coordinates (Inf) detected\n");
      return &identity;
    }

    // First four equations for x coordinates
    A[i][0] = srcX;
    A[i][1] = srcY;
    A[i][2] = 1.0;
    A[i][6] = -srcX * dstX;
    A[i][7] = -srcY * dstX;
    b[i] = dstX;

    // Last four equations for y coordinates
    A[i+4][3] = srcX;
    A[i+4][4] = srcY;
    A[i+4][5] = 1.0;
    A[i+4][6] = -srcX * dstY;
    A[i+4][7] = -srcY * dstY;
    b[i+4] = dstY;
  }

  log("Solve the system of equations …\n");
  if (!solve_linear_system(A, b, x)) {
    log("Failed to solve system, returning identity matrix\n");
    return &identity;
  }

  // Validate solution
  for (int i = 0; i < 8; i++) {
    if (isnan(x[i]) || isinf(x[i]) || fabs(x[i]) > 1e6) {
      log("Error: Invalid solution values detected\n");
      return &identity;
    }
  }

  Matrix3x3* result = malloc(sizeof(Matrix3x3));
  *result = (Matrix3x3) {
    x[0], x[1], x[2],
    x[3], x[4], x[5],
    x[6], x[7], 1.0
  };

  #ifdef DEBUG_LOGGING
    printf("Result matrix:\n");
    printf("%f, %f, %f\n", result->m00, result->m01, result->m02);
    printf("%f, %f, %f\n", result->m10, result->m11, result->m12);
    printf("%f, %f, %f\n", result->m20, result->m21, result->m22);
  #endif

  // Final validation of the result matrix
  if (
    isnan(result->m00) || isnan(result->m01) || isnan(result->m02) ||
    isnan(result->m10) || isnan(result->m11) || isnan(result->m12) ||
    isnan(result->m20) || isnan(result->m21) || isnan(result->m22) ||
    isinf(result->m00) || isinf(result->m01) || isinf(result->m02) ||
    isinf(result->m10) || isinf(result->m11) || isinf(result->m12) ||
    isinf(result->m20) || isinf(result->m21) || isinf(result->m22)
  ) {
    log("Error: Invalid values in result matrix\n");
    return &identity;
  }

  return result;
}
