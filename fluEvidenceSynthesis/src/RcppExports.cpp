#include "rcppwrap.hh"
// This file was generated by Rcpp::compileAttributes
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include <RcppEigen.h>
#include <Rcpp.h>

using namespace Rcpp;

// cholesky
Eigen::MatrixXd cholesky(SEXP A);
RcppExport SEXP fluEvidenceSynthesis_cholesky(SEXP ASEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< SEXP >::type A(ASEXP);
    __result = Rcpp::wrap(cholesky(A));
    return __result;
END_RCPP
}
// convertVaccineCalendar
RcppExport SEXP convertVaccineCalendar(SEXP vaccine_calendar);
RcppExport SEXP fluEvidenceSynthesis_convertVaccineCalendar(SEXP vaccine_calendarSEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< SEXP >::type vaccine_calendar(vaccine_calendarSEXP);
    __result = Rcpp::wrap(convertVaccineCalendar(vaccine_calendar));
    return __result;
END_RCPP
}
// summine
int summine(int a, int b);
RcppExport SEXP fluEvidenceSynthesis_summine(SEXP aSEXP, SEXP bSEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< int >::type a(aSEXP);
    Rcpp::traits::input_parameter< int >::type b(bSEXP);
    __result = Rcpp::wrap(summine(a, b));
    return __result;
END_RCPP
}
// vaccinationScenario
bool vaccinationScenario(std::vector<size_t> age_sizes, vaccine_t vaccine_calendar);
RcppExport SEXP fluEvidenceSynthesis_vaccinationScenario(SEXP age_sizesSEXP, SEXP vaccine_calendarSEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< std::vector<size_t> >::type age_sizes(age_sizesSEXP);
    Rcpp::traits::input_parameter< vaccine_t >::type vaccine_calendar(vaccine_calendarSEXP);
    __result = Rcpp::wrap(vaccinationScenario(age_sizes, vaccine_calendar));
    return __result;
END_RCPP
}
