#include <algorithm>
#include <cstdio>

#include "HCheckConfig.h"
#include "Highs.h"
#include "catch.hpp"
//#include "io/FilereaderLp.h"

const bool dev_run = true;
const double inf = kHighsInf;

void testIis(const std::string& model, const HighsIis& iis) {
  HighsModelStatus model_status;
  std::string model_file =
      std::string(HIGHS_DIR) + "/check/instances/" + model + ".mps";
  HighsInt num_iis_col = iis.col_index_.size();
  HighsInt num_iis_row = iis.row_index_.size();

  Highs highs;
  highs.setOptionValue("output_flag", false);
  REQUIRE(highs.readModel(model_file) == HighsStatus::kOk);
  const HighsLp& incumbent_lp = highs.getLp();
  HighsLp lp = highs.getLp();
  // Zero the objective
  lp.col_cost_.assign(lp.num_col_, 0);
  REQUIRE(highs.changeColsCost(0, lp.num_col_ - 1, lp.col_cost_.data()) ==
          HighsStatus::kOk);

  // Save the bounds
  std::vector<double> iis_col_lower;
  std::vector<double> iis_col_upper;
  std::vector<double> iis_row_lower;
  std::vector<double> iis_row_upper;
  for (HighsInt iisCol = 0; iisCol < num_iis_col; iisCol++) {
    HighsInt iCol = iis.col_index_[iisCol];
    iis_col_lower.push_back(lp.col_lower_[iCol]);
    iis_col_upper.push_back(lp.col_upper_[iCol]);
  }
  for (HighsInt iisRow = 0; iisRow < num_iis_row; iisRow++) {
    HighsInt iRow = iis.row_index_[iisRow];
    iis_row_lower.push_back(lp.row_lower_[iRow]);
    iis_row_upper.push_back(lp.row_upper_[iRow]);
  }

  // Free all the columns and rows
  lp.col_lower_.assign(lp.num_col_, -inf);
  lp.col_upper_.assign(lp.num_col_, inf);
  lp.row_lower_.assign(lp.num_row_, -inf);
  lp.row_upper_.assign(lp.num_row_, inf);
  // Restore the bounds for the IIS columns and rows
  for (HighsInt iisCol = 0; iisCol < num_iis_col; iisCol++) {
    HighsInt iCol = iis.col_index_[iisCol];
    lp.col_lower_[iCol] = iis_col_lower[iisCol];
    lp.col_upper_[iCol] = iis_col_upper[iisCol];
  }
  for (HighsInt iisRow = 0; iisRow < num_iis_row; iisRow++) {
    HighsInt iRow = iis.row_index_[iisRow];
    lp.row_lower_[iRow] = iis_row_lower[iisRow];
    lp.row_upper_[iRow] = iis_row_upper[iisRow];
  }

  highs.passModel(lp);
  highs.run();
  model_status = highs.getModelStatus();
  printf("IIS LP yields model status %s\n",
         highs.modelStatusToString(model_status).c_str());
  REQUIRE(model_status == HighsModelStatus::kInfeasible);

  for (HighsInt iisCol = 0; iisCol < num_iis_col; iisCol++) {
    HighsInt iCol = iis.col_index_[iisCol];
    HighsInt iis_bound = iis.col_bound_[iisCol];
    const double lower = lp.col_lower_[iCol];
    const double upper = lp.col_upper_[iCol];
    double to_lower = lower;
    double to_upper = upper;
    REQUIRE(iis_bound != kIisBoundStatusDropped);
    REQUIRE(iis_bound != kIisBoundStatusNull);
    REQUIRE(iis_bound != kIisBoundStatusBoxed);
    if (iis_bound == kIisBoundStatusLower) {
      to_lower = -inf;
    } else if (iis_bound == kIisBoundStatusUpper) {
      to_upper = inf;
    } else if (iis_bound == kIisBoundStatusFree) {
      printf("IIS Col %2d (LP col %6d) status %s\n", int(iisCol), int(iCol),
             iis.iisBoundStatusToString(iis_bound).c_str());
      continue;
    }
    REQUIRE(highs.changeColBounds(iCol, to_lower, to_upper) ==
            HighsStatus::kOk);
    REQUIRE(highs.run() == HighsStatus::kOk);
    model_status = highs.getModelStatus();
    if (dev_run)
      printf(
          "IIS Col %2d (LP col %6d) status %s: removal yields model status "
          "%s\n",
          int(iisCol), int(iCol), iis.iisBoundStatusToString(iis_bound).c_str(),
          highs.modelStatusToString(model_status).c_str());
    //    if (model_status != HighsModelStatus::kOptimal)
    //    highs.writeSolution("", kSolutionStylePretty);
    REQUIRE(model_status == HighsModelStatus::kOptimal);
    REQUIRE(highs.changeColBounds(iCol, lower, upper) == HighsStatus::kOk);
  }
  for (HighsInt iisRow = 0; iisRow < num_iis_row; iisRow++) {
    HighsInt iRow = iis.row_index_[iisRow];
    HighsInt iis_bound = iis.row_bound_[iisRow];
    const double lower = lp.row_lower_[iRow];
    const double upper = lp.row_upper_[iRow];
    double to_lower = lower;
    double to_upper = upper;
    REQUIRE(iis_bound != kIisBoundStatusDropped);
    REQUIRE(iis_bound != kIisBoundStatusNull);
    REQUIRE(iis_bound != kIisBoundStatusFree);
    REQUIRE(iis_bound != kIisBoundStatusBoxed);
    if (iis_bound == kIisBoundStatusLower) {
      to_lower = -inf;
    } else if (iis_bound == kIisBoundStatusUpper) {
      to_upper = inf;
    }
    REQUIRE(highs.changeRowBounds(iRow, to_lower, to_upper) ==
            HighsStatus::kOk);
    REQUIRE(highs.run() == HighsStatus::kOk);
    model_status = highs.getModelStatus();
    if (dev_run)
      printf(
          "IIS Row %2d (LP row %6d) status %s: removal yields model status "
          "%s\n",
          int(iisRow), int(iRow), iis.iisBoundStatusToString(iis_bound).c_str(),
          highs.modelStatusToString(model_status).c_str());
    //    if (model_status != HighsModelStatus::kOptimal)
    //    highs.writeSolution("", kSolutionStylePretty);
    REQUIRE(model_status == HighsModelStatus::kOptimal);
    REQUIRE(highs.changeRowBounds(iRow, lower, upper) == HighsStatus::kOk);
  }
}

void testMps(std::string& model, const HighsInt iis_strategy) {
  std::string model_file =
      std::string(HIGHS_DIR) + "/check/instances/" + model + ".mps";
  Highs highs;
  highs.setOptionValue("output_flag", dev_run);

  REQUIRE(highs.readModel(model_file) == HighsStatus::kOk);
  highs.writeModel("");
  REQUIRE(highs.run() == HighsStatus::kOk);
  REQUIRE(highs.getModelStatus() == HighsModelStatus::kInfeasible);
  highs.setOptionValue("iis_strategy", iis_strategy);
  REQUIRE(highs.computeIis() == HighsStatus::kOk);
  const HighsIis& iis = highs.getIis();
  HighsInt num_iis_col = iis.col_index_.size();
  HighsInt num_iis_row = iis.row_index_.size();
  REQUIRE(num_iis_col > 0);
  REQUIRE(num_iis_row > 0);
  if (dev_run)
    printf("Model %s has IIS with %d columns and %d rows\n", model.c_str(),
           int(num_iis_col), int(num_iis_row));
  testIis(model, iis);
}

TEST_CASE("lp-incompatible-bounds", "[iis]") {
  // LP has row0 and col2 with inconsistent bounds.
  //
  // When prioritising rows, row0 and its constituent columns (1, 2) should be
  // found
  //
  // When prioritising columns, col2 and its constituent rows (0, 1) should be
  // found
  HighsLp lp;
  lp.num_col_ = 3;
  lp.num_row_ = 2;
  lp.col_cost_ = {0, 0, 0};
  lp.col_lower_ = {0, 0, 0};
  lp.col_upper_ = {1, 1, -1};
  lp.row_lower_ = {1, 0};
  lp.row_upper_ = {0, 1};
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.a_matrix_.start_ = {0, 2, 4};
  lp.a_matrix_.index_ = {1, 2, 0, 2};
  lp.a_matrix_.value_ = {1, 1, 1, 1};
  Highs highs;
  highs.setOptionValue("output_flag", dev_run);
  highs.passModel(lp);
  REQUIRE(highs.run() == HighsStatus::kOk);
  REQUIRE(highs.getModelStatus() == HighsModelStatus::kInfeasible);
  HighsInt num_iis_col;
  HighsInt num_iis_row;
  std::vector<HighsInt> iis_col_index;
  std::vector<HighsInt> iis_row_index;
  std::vector<HighsInt> iis_col_bound;
  std::vector<HighsInt> iis_row_bound;
  highs.setOptionValue("iis_strategy", kIisStrategyFromLpRowPriority);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row) == HighsStatus::kOk);
  REQUIRE(num_iis_col == 0);
  REQUIRE(num_iis_row == 1);
  iis_col_index.resize(num_iis_col);
  iis_row_index.resize(num_iis_row);
  iis_col_bound.resize(num_iis_col);
  iis_row_bound.resize(num_iis_row);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row, iis_col_index.data(),
                       iis_row_index.data(), iis_col_bound.data(),
                       iis_row_bound.data()) == HighsStatus::kOk);
  REQUIRE(iis_row_index[0] == 0);
  REQUIRE(iis_row_bound[0] == kIisBoundStatusBoxed);
  highs.setOptionValue("iis_strategy", kIisStrategyFromLpColPriority);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row) == HighsStatus::kOk);
  REQUIRE(num_iis_col == 1);
  REQUIRE(num_iis_row == 0);
  iis_col_index.resize(num_iis_col);
  iis_row_index.resize(num_iis_row);
  iis_col_bound.resize(num_iis_col);
  iis_row_bound.resize(num_iis_row);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row, iis_col_index.data(),
                       iis_row_index.data(), iis_col_bound.data(),
                       iis_row_bound.data()) == HighsStatus::kOk);
  REQUIRE(iis_col_index[0] == 2);
  REQUIRE(iis_col_bound[0] == kIisBoundStatusBoxed);
}

TEST_CASE("lp-empty-infeasible-row", "[iis]") {
  // Second row is empty, with bounds of [1, 2]
  const HighsInt empty_row = 1;
  HighsLp lp;
  lp.num_col_ = 2;
  lp.num_row_ = 3;
  lp.col_cost_ = {0, 0};
  lp.col_lower_ = {0, 0};
  lp.col_upper_ = {inf, inf};
  lp.row_lower_ = {-inf, 1, -inf};
  lp.row_upper_ = {8, 2, 9};
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.a_matrix_.start_ = {0, 2, 2, 4};
  lp.a_matrix_.index_ = {0, 1, 0, 1};
  lp.a_matrix_.value_ = {2, 1, 1, 3};
  Highs highs;
  highs.setOptionValue("output_flag", dev_run);
  highs.passModel(lp);
  REQUIRE(highs.run() == HighsStatus::kOk);
  REQUIRE(highs.getModelStatus() == HighsModelStatus::kInfeasible);
  HighsInt num_iis_col;
  HighsInt num_iis_row;
  std::vector<HighsInt> iis_col_index;
  std::vector<HighsInt> iis_row_index;
  std::vector<HighsInt> iis_col_bound;
  std::vector<HighsInt> iis_row_bound;
  REQUIRE(highs.getIis(num_iis_col, num_iis_row) == HighsStatus::kOk);
  REQUIRE(num_iis_col == 0);
  REQUIRE(num_iis_row == 1);
  iis_col_index.resize(num_iis_col);
  iis_row_index.resize(num_iis_row);
  iis_col_bound.resize(num_iis_col);
  iis_row_bound.resize(num_iis_row);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row, iis_col_index.data(),
                       iis_row_index.data(), iis_col_bound.data(),
                       iis_row_bound.data()) == HighsStatus::kOk);
  REQUIRE(iis_row_index[0] == empty_row);
  REQUIRE(iis_row_bound[0] == kIisBoundStatusLower);
  REQUIRE(highs.changeRowBounds(empty_row, -2, -1) == HighsStatus::kOk);
  REQUIRE(highs.run() == HighsStatus::kOk);
  REQUIRE(highs.getModelStatus() == HighsModelStatus::kInfeasible);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row) == HighsStatus::kOk);
  REQUIRE(num_iis_col == 0);
  REQUIRE(num_iis_row == 1);
  iis_col_index.resize(num_iis_col);
  iis_row_index.resize(num_iis_row);
  iis_col_bound.resize(num_iis_col);
  iis_row_bound.resize(num_iis_row);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row, iis_col_index.data(),
                       iis_row_index.data(), iis_col_bound.data(),
                       iis_row_bound.data()) == HighsStatus::kOk);
  REQUIRE(iis_row_index[0] == empty_row);
  REQUIRE(iis_row_bound[0] == kIisBoundStatusUpper);
}

TEST_CASE("lp-get-iis", "[iis]") {
  HighsLp lp;
  lp.num_col_ = 2;
  lp.num_row_ = 3;
  lp.col_cost_ = {0, 0};
  lp.col_lower_ = {0, 0};
  lp.col_upper_ = {inf, inf};
  lp.row_lower_ = {-inf, -inf, -inf};
  lp.row_upper_ = {8, 9, -2};
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.a_matrix_.start_ = {0, 2, 4, 6};
  lp.a_matrix_.index_ = {0, 1, 0, 1, 0, 1};
  lp.a_matrix_.value_ = {2, 1, 1, 3, 1, 1};
  Highs highs;
  highs.setOptionValue("output_flag", dev_run);
  highs.passModel(lp);
  REQUIRE(highs.run() == HighsStatus::kOk);
  REQUIRE(highs.getModelStatus() == HighsModelStatus::kInfeasible);
  HighsInt num_iis_col;
  HighsInt num_iis_row;
  std::vector<HighsInt> iis_col_index;
  std::vector<HighsInt> iis_row_index;
  std::vector<HighsInt> iis_col_bound;
  std::vector<HighsInt> iis_row_bound;
  highs.setOptionValue("iis_strategy", kIisStrategyFromLpRowPriority);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row) == HighsStatus::kOk);
  REQUIRE(num_iis_col == 2);
  REQUIRE(num_iis_row == 1);
  iis_col_index.resize(num_iis_col);
  iis_row_index.resize(num_iis_row);
  iis_col_bound.resize(num_iis_col);
  iis_row_bound.resize(num_iis_row);
  REQUIRE(highs.getIis(num_iis_col, num_iis_row, iis_col_index.data(),
                       iis_row_index.data(),
                       // iis_col_bound.data(), iis_row_bound.data()
                       nullptr, nullptr) == HighsStatus::kOk);
  REQUIRE(iis_col_index[0] == 0);
  REQUIRE(iis_col_index[1] == 1);
  REQUIRE(iis_row_index[0] == 2);
}

TEST_CASE("lp-get-iis-woodinfe", "[iis]") {
  std::string model = "woodinfe";
  testMps(model, kIisStrategyFromRayRowPriority);
}

TEST_CASE("lp-get-iis-galenet", "[iis]") {
  // Dual ray corresponds to constraints
  //
  // r0: 0  <= c0 + c1      - c3 - c4 <=0
  //
  // r1: 20 <=           c2 + c3
  //
  // r2: 30 <=                     c4
  //
  // Where
  //
  // 0 <= c0 <= 10
  //
  // 0 <= c1 <= 10
  //
  // 0 <= c2 <=  2
  //
  // 0 <= c3 <= 20
  //
  // 0 <= c4 <= 30
  //
  // This is infeasible since c4 >= 30 and c4 <= 30 fices c4 = 30,
  // then c0 + c1 >= c3 + c4 >= 30 cannot be satisfied due to the
  // upper bounds of 10 on these variables
  //
  // r1 can be removed and infeasibility is retained, but not r0 or r2
  //
  // The upper bound on r0 can be removed
  //
  // The lower bounds on c0 and c1 can be removed, but not their upper
  // bounds
  //
  // c2 can be removed, as it is empty once r1 is removed
  //
  // c3 can be removed, as the value of c4 is sufficient to make r0
  // infeasible
  //
  // The bounds on c4 can be removed, since it's the lower bound from
  // r2 that makes r0 infeasible
  //
  // Hence only empty columns can be removed
  std::string model = "galenet";
  testMps(model, kIisStrategyFromLpRowPriority);
  testMps(model, kIisStrategyFromRayRowPriority);
}
