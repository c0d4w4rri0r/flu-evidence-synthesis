#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <iostream>

#include "model.h"
#include "state.h"
#include "data.h"
#include "contacts.h"
#include "vaccine.h"
#include "proposal.h"

#include "mcmc.h"

#include "rcppwrap.h"
#include<RcppEigen.h>

using namespace flu;

//' MCMC based inference of the parameter values given the different data sets
//'
//' @param demography A vector with the population size by each age {0,1,..}
//' @param ili The number of Influenza-like illness cases per week
//' @param mon_pop The number of people monitored for ili
//' @param n_pos The number of positive samples for the given strain (per week)
//' @param n_samples The total number of samples tested 
//' @param vaccine_calendar A vaccine calendar valid for that year
//' @param polymod_data Contact data for different age groups
//' @param initial Vector with starting parameter values
//' @param mapping Group mapping from model groups to data groups
//' @param risk_ratios Risk ratios to convert to and from population groups
//' @param no_age_groups Number of age groups
//' @param no_risk_groups Number of risk groups
//' @param mapping Group mapping from model groups to data groups
//' @param nburn Number of iterations of burn in
//' @param nbatch Number of batches to run (number of samples to return)
//' @param blen Length of each batch
//' 
//' @return Returns a list with the accepted samples and the corresponding llikelihood values and a matrix (contact.ids) containing the ids (row number) of the contacts data used to build the contact matrix.
//'
// [[Rcpp::export(name=".inference_cpp")]]
mcmc_result_inference_t inference_cpp( std::vector<size_t> demography,
        std::vector<size_t> age_group_limits,
        Eigen::MatrixXi ili, Eigen::MatrixXi mon_pop, 
        Eigen::MatrixXi n_pos, Eigen::MatrixXi n_samples, 
        flu::vaccine::vaccine_t vaccine_calendar,
        Eigen::MatrixXi polymod_data,
        Eigen::VectorXd initial,
        Eigen::MatrixXd mapping,
        Eigen::VectorXd risk_ratios,
        Eigen::VectorXd epsilon_index,
        size_t psi_index,
        size_t transmissibility_index,
        Eigen::VectorXd susceptibility_index,
        size_t initial_infected_index,
        Rcpp::Function lprior,
        bool pass_prior,
        size_t no_age_groups,
        size_t no_risk_groups,
        bool uk_prior,
        size_t nburn = 0,
        size_t nbatch = 1000, size_t blen = 1 )
{
    //Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor> mapping,
    mcmc_result_inference_t results;
    results.batch = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>( nbatch, initial.size() );
    results.llikelihoods = Eigen::VectorXd( nbatch );
    results.contact_ids = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>( nbatch, polymod_data.rows() );

    int step_mat;
    double prop_likelihood;

    double my_acceptance_rate;

    flu::data::age_data_t age_data;
    age_data.age_sizes = demography;
    age_data.age_group_sizes = flu::data::group_age_data( demography,
            age_group_limits );

    auto pop_vec = flu::data::stratify_by_risk( 
            age_data.age_group_sizes, risk_ratios, no_risk_groups);

    /*pop RCGP*/
    //std::vector<double> pop_RCGP = std::vector<double>(5);
    Eigen::VectorXd pop_RCGP = Eigen::VectorXd::Zero(ili.cols());
    for (int i = 0; i < mapping.rows(); ++i) {
      // to_j = weight*from_i
      pop_RCGP((size_t) mapping(i,1)) += mapping(i,2) * pop_vec((size_t) mapping(i,0)); 
    }


    /********************************************************************************************************************************************************
    initialisation point to start the MCMC
    *********************************************************************************************************************************************************/

    auto pars_to_epsilon = [&epsilon_index]( const Eigen::VectorXd &pars )
    {
        Eigen::VectorXd eps(epsilon_index.size());
        for (size_t i = 0; i < epsilon_index.size(); ++i)
            eps[i] = pars[epsilon_index[i]];
        return eps;
    };

    auto pars_to_susceptibility = [&susceptibility_index]( const Eigen::VectorXd &pars )
    {
        Eigen::VectorXd susc(susceptibility_index.size());
        for (size_t i = 0; i < susceptibility_index.size(); ++i)
            susc[i] = pars[susceptibility_index[i]];
        return susc;
    };

    /*translate into an initial infected population*/
    std::vector<size_t> contact_ids;
    for (size_t i = 0; i < (size_t)polymod_data.rows(); ++i)
        contact_ids.push_back(i+1);

    auto curr_parameters = initial;
    Eigen::VectorXd curr_init_inf = flu::data::stratify_by_risk(
            Eigen::VectorXd::Constant(no_age_groups, pow(10,curr_parameters[initial_infected_index]) ),
            risk_ratios, no_risk_groups);

    if (no_risk_groups < 3)
    {
        pop_vec.conservativeResize(no_age_groups*3);
        curr_init_inf.conservativeResize(no_age_groups*3);
        for( size_t i = no_age_groups*no_risk_groups; i<pop_vec.size(); ++i)
        {
            pop_vec[i] = 0;
            curr_init_inf[i] = 0;
        }
    } else if (no_risk_groups > 3)
        ::Rf_error("Maximum of three risk groups supported");

    auto polymod = flu::contacts::table_to_contacts( polymod_data, 
            age_group_limits ); 

    auto curr_c = contacts::shuffle_by_id( polymod, 
            contact_ids );

    auto current_contact_regular = 
        contacts::to_symmetric_matrix( curr_c, age_data );

    auto time_latent = 0.8;
    auto time_infectious = 1.8;
        
    Eigen::VectorXd curr_init_resist = Eigen::VectorXd::Zero(curr_init_inf.col());

    auto result = infectionODE(pop_vec, 
            curr_init_inf,
            curr_init_resist,
            time_latent, time_infectious, 
            pars_to_susceptibility(curr_parameters),
            current_contact_regular, curr_parameters[transmissibility_index], 
            vaccine_calendar, 7*24 );
    /*curr_psi=0.00001;*/
    auto d_app = 3;
    auto curr_llikelihood = log_likelihood_hyper_poisson(
            pars_to_epsilon(curr_parameters),
            curr_parameters[psi_index], 
            days_to_weeks_5AG(result, mapping, pop_RCGP.size()), 
            ili, mon_pop, n_pos, n_samples, pop_RCGP, d_app);

    auto proposal_state = proposal::initialize( curr_parameters.size() );

    double curr_prior = 0;
    double prop_prior = 0;
    auto Rlprior = [&lprior]( const Eigen::VectorXd &pars ) {
        double lPrior = Rcpp::as<double>(lprior( pars ));
        return lPrior;
    };

    auto log_prior_ratio_f = [pass_prior, &Rlprior, &prop_prior, &curr_prior, uk_prior, &epsilon_index, psi_index, transmissibility_index, &susceptibility_index, 
         initial_infected_index](const Eigen::VectorXd &proposed, const Eigen::VectorXd &current, bool susceptibility) {
             if (uk_prior)
                return log_prior(proposed, current, susceptibility);
             else if (pass_prior) {
                 prop_prior = Rlprior(proposed);
                 return prop_prior - curr_prior;
             } else {
                 prop_prior = 0;
                 // Use flat priors
                 for (auto i = 0; i < epsilon_index.size(); ++i) {
                     auto index = epsilon_index[i];
                     if (proposed[index] < 0 || proposed[index] > 1) {
                         prop_prior = log(0);
                         break;
                     }
                 }
                 if (proposed[psi_index] < 0 || proposed[transmissibility_index] < 0)
                     prop_prior = log(0);
                 for (auto i = 0; i < susceptibility_index.size(); ++i) {
                     auto index = susceptibility_index[i];
                     if (!std::isfinite(prop_prior) || proposed[index] < 0 || proposed[index] > 1) {
                         prop_prior = log(0);
                         break;
                     }
                 }
                 return prop_prior - curr_prior;
             }
         };

    /**************************************************************************************************************************************************************
    Start of the MCMC
    **************************************************************************************************************************************************************/

    step_mat=1;            /*number of contacts exchanged*/
    double p_ac_mat=0.10;          /*prob to redraw matrices*/

    size_t sampleCount = 0;
    int k = 0;

    while(sampleCount<nbatch)
    {
        ++k;

        /*update of the variance-covariance matrix and the mean vector*/
        proposal_state = proposal::update( std::move( proposal_state ),
                curr_parameters, k );
     
        /*
        if (k>=nburn)
        {
          Rcpp::Rcout << "Adaptive scaling: " << proposal_state.adaptive_scaling << std::endl;
          Rcpp::Rcout << "past_acceptance: " << proposal_state.past_acceptance << std::endl;
          Rcpp::Rcout << "conv_scaling: " << proposal_state.conv_scaling << std::endl;
          Rcpp::Rcout << "Acceptance: " << proposal_state.acceptance << std::endl;
          Rcpp::Rcout << proposal_state.emp_cov_matrix << std::endl << std::endl;
        }
        */
        /*
        auto prop_parameters = proposal::haario_adapt_scale(
                curr_parameters,
                proposal_state.chol_emp_cov,
                proposal_state.chol_ini,0.05, 
                proposal_state.adaptive_scaling );*/

        auto prop_parameters = proposal::sherlock( k,
                curr_parameters,
                proposal_state );

        auto prior_ratio = 
            log_prior_ratio_f(prop_parameters, curr_parameters, false );

        if (!std::isfinite(prior_ratio))
        {
            //Rcpp::Rcout << "Invalid proposed par" << std::endl;
            // TODO: code duplication with failure of acceptance
            proposal_state = proposal::accepted( 
                    std::move(proposal_state), 
                    false, k );
        } else {
            /*translate into an initial infected population*/
            
            Eigen::VectorXd prop_init_inf = flu::data::stratify_by_risk(
                    Eigen::VectorXd::Constant(no_age_groups, pow(10,prop_parameters[initial_infected_index]) ),
                    risk_ratios, no_risk_groups);
            if (no_risk_groups < 3)
            {
                prop_init_inf.conservativeResize(no_age_groups*3);
                for( size_t i = no_age_groups*no_risk_groups; i<prop_init_inf.size(); ++i)
                {
                    prop_init_inf[i] = 0;
                }
            }
            auto prop_c = curr_c;

            /*do swap of contacts step_mat times (reduce or increase to change 'distance' of new matrix from current)*/
            // TODO/WARN Need to draw this before hand and pass it as data to
            // likelihood function... Even when doing that we still need to know k,
            // so might as well make the likelihood function increase k when called
            if(R::runif(0,1) < p_ac_mat)
                prop_c = contacts::bootstrap_contacts( std::move(prop_c),
                        polymod, step_mat );

            auto prop_contact_regular = 
                contacts::to_symmetric_matrix( prop_c, age_data );
                
            Eigen::VectorXd prop_init_resist = Eigen::VectorXd::Zero(prop_init_inf.col());

            result = infectionODE(pop_vec, 
                    prop_init_inf, 
                    prop_init_resist,
                    time_latent, time_infectious, 
                    pars_to_susceptibility(prop_parameters),
                    prop_contact_regular, prop_parameters[transmissibility_index], 
                    vaccine_calendar, 7*24 );

            /*computes the associated likelihood with the proposed values*/
            prop_likelihood=log_likelihood_hyper_poisson(
                    pars_to_epsilon(prop_parameters), 
                    prop_parameters[psi_index], 
                    days_to_weeks_5AG(result, mapping, pop_RCGP.size()), 
                    ili, mon_pop, n_pos, n_samples, pop_RCGP, d_app);

            /*Acceptance rate include the likelihood and the prior but no correction for the proposal as we use a symmetrical RW*/
            // Make sure accept works with -inf prior
            // MCMC-R alternative prior?
            if (std::isinf(prop_likelihood) && std::isinf(curr_llikelihood) )
                my_acceptance_rate = exp(prior_ratio); // We want to explore and find a non infinite likelihood
            else 
                my_acceptance_rate=
                    exp(prop_likelihood-curr_llikelihood+
                    prior_ratio);

            if(R::runif(0,1)<my_acceptance_rate) /*with prior*/
            {
                /*update the acceptance rate*/
                proposal_state = proposal::accepted( 
                        std::move(proposal_state), true, k );

                curr_prior = prop_prior;
                curr_parameters = prop_parameters;

                /*update current likelihood*/
                curr_llikelihood=prop_likelihood;

                /*new proposed contact matrix*/
                /*update*/
                curr_c = prop_c;
                current_contact_regular=prop_contact_regular;
            }
            else /*if reject*/
            {
                proposal_state = proposal::accepted( 
                        std::move(proposal_state), false, k );
            }
        }

        if(k%blen==0 && k>=(int)nburn)
        {
            // Add results
            results.llikelihoods[sampleCount] = curr_llikelihood;
            results.batch.row( sampleCount ) = curr_parameters;
            for( size_t i = 0; i < curr_c.contacts.size(); ++i )
                results.contact_ids( sampleCount, i ) =
                    curr_c.contacts[i].id;

            ++sampleCount;
        }
    }
    return results;
}

double dmultinomial( const Eigen::VectorXi &x, int size, 
        const Eigen::VectorXd &prob, 
        bool use_log = false )
{
    double loglik = 0.0;

    loglik = R::lgammafn(size+1);

    for (size_t i=0; i < x.size(); ++i)
        loglik += x[i]*log(prob[i]) - R::lgammafn(x[i]+1);
    if (use_log)
        return loglik;
    return exp(loglik);
}

//' Probability density function for multinomial distribution
//'
//' @param x The counts
//' @param size The total size from which is being samples
//' @param prob Probabilities of each different outcome
//' @param use_log Whether to return logarithm probability
//'
//' @return The probability of getting the counts, given the total size and probability of drawing each.
//'
// [[Rcpp::export(name="dmultinom.cpp")]]
double dmultinomialCPP( Eigen::VectorXi x, int size, Eigen::VectorXd prob, 
        bool use_log = false )
{
    return dmultinomial( x, size, prob, use_log );
}

//' MCMC based inference of the parameter values given the different data sets based on multiple strains
//'
//' @param demography A vector with the population size by each age {1,2,..}
//' @param ili The number of Influenza-like illness cases per week
//' @param mon_pop The number of people monitored for ili
//' @param n_pos The number of positive samples per strain (per week)
//' @param n_samples The total number of samples tested 
//' @param vaccine_calendar Vaccine calendars per strain valid for that year
//' @param polymod_data Contact data for different age groups
//' @param initial Vector with starting parameter values
//' @param nburn Number of iterations of burn in
//' @param nbatch Number of batches to run (number of samples to return)
//' @param blen Length of each batch
//' 
//' @return Returns a list with the accepted samples and the corresponding llikelihood values and a matrix (contact.ids) containing the ids (row number) of the contacts data used to build the contact matrix.
//'
// [[Rcpp::export]]
mcmc_result_inference_t inference_multistrains( 
        std::vector<size_t> demography, 
        Eigen::MatrixXi ili, Eigen::MatrixXi mon_pop, 
        Rcpp::List n_pos, Eigen::MatrixXi n_samples, 
        Rcpp::List vaccine_calendar,
        Eigen::MatrixXi polymod_data,
        Eigen::VectorXd initial, 
        size_t nburn = 0,
        size_t nbatch = 1000, size_t blen = 1 )
{
    mcmc_result_inference_t results;
    results.batch = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>( nbatch, initial.size() );
    results.llikelihoods = Eigen::VectorXd( nbatch );
    results.contact_ids = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>( nbatch, polymod_data.rows() );

    //create vaccine_calendars flu::vaccine::vaccine_t
    std::vector<flu::vaccine::vaccine_t> vaccine_calendars;
    for ( auto vc : vaccine_calendar )
        vaccine_calendars.push_back(Rcpp::as<flu::vaccine::vaccine_t>( vc ));

    //create positives;
    std::vector<Eigen::MatrixXd> positives;
    for ( auto p : n_pos )
        positives.push_back( Rcpp::as<Eigen::MatrixXd>( p ) );

    
    auto nag = 7;
    auto no_strains = n_pos.size();

    int step_mat;
    double pop_RCGP[5];

    double my_acceptance_rate;

    std::vector<size_t> age_group_limits = {1,5,15,25,45,65};

    flu::data::age_data_t age_data;
    age_data.age_sizes = demography;
    age_data.age_group_sizes = flu::data::group_age_data( demography,
            age_group_limits );

    Eigen::MatrixXd risk_proportions = Eigen::MatrixXd( 
            2, age_data.age_group_sizes.size() );
    risk_proportions << 
        0.021, 0.055, 0.098, 0.087, 0.092, 0.183, 0.45, 
        0, 0, 0, 0, 0, 0, 0;

    auto pop_vec = flu::data::separate_into_risk_groups( 
            age_data.age_group_sizes, risk_proportions  );

    /*pop RCGP*/
    pop_RCGP[0]=pop_vec[0]+pop_vec[1]+pop_vec[7]+pop_vec[8]+pop_vec[14]+pop_vec[15];
    pop_RCGP[1]=pop_vec[2]+pop_vec[9]+pop_vec[16];
    pop_RCGP[2]=pop_vec[3]+pop_vec[4]+pop_vec[10]+pop_vec[11]+pop_vec[17]+pop_vec[18];
    pop_RCGP[3]=pop_vec[5]+pop_vec[12]+pop_vec[19];
    pop_RCGP[4]=pop_vec[6]+pop_vec[13]+pop_vec[20];

    /********************************************************************************************************************************************************
    initialisation point to start the MCMC
    *********************************************************************************************************************************************************/
    /*translate into an initial infected population*/
    std::vector<size_t> contact_ids;
    for (size_t i = 0; i < (size_t)polymod_data.rows(); ++i)
        contact_ids.push_back(i+1);

    auto curr_parameters = initial;
    auto polymod = flu::contacts::table_to_contacts( polymod_data, 
            age_group_limits ); 

    auto curr_c = contacts::shuffle_by_id( polymod, 
            contact_ids );

    auto current_contact_regular = 
        contacts::to_symmetric_matrix( curr_c, age_data );

    auto time_latent = 0.8;
    auto time_infectious = 1.8;

    auto lprior_function = [&no_strains]( const Eigen::VectorXd &pars )
    {
        if (pars[no_strains*8]<0)
            return log(0);
        auto lprob = 0.0;

        for ( size_t i = 0; i < (size_t)no_strains; ++i )
        {
            auto sub_pars = pars.segment( i*8, 8 );

            if( 
                    sub_pars[0] <= 0 || sub_pars[0] >= 1 ||
                    sub_pars[1] <= 0 || sub_pars[1] >= 1 ||
                    sub_pars[2] <= 0 || sub_pars[2] >= 1 ||
                    sub_pars[3] < 0 ||
                    sub_pars[4] < 0 || sub_pars[4] > 1 ||
                    sub_pars[5] < 0 || sub_pars[5] > 1 ||
                    sub_pars[6] < 0 || sub_pars[6] > 1 ||
                    sub_pars[7]<log(0.00001) || sub_pars[7]>log(10)
              )
                return log(0);

            /*Prior for the transmissibility; year other than 2003/04*/
            /*correction for a normal prior with mu=0.1653183 and sd=0.02773053*/
            /*prior on q*/
            lprob += R::dnorm(sub_pars[3],0.1653183,0.02773053,1);
            /*Prior for the ascertainment probabilities*/

            /*correct for the prior from serology season (lognormal):"0-14" lm=-4.493789, ls=0.2860455*/
            lprob += R::dlnorm(sub_pars[0], -4.493789,0.2860455, 1);
            /*correct for the prior from serology season (lognormal):"15-64" lm=-4.117028, ls=0.4751615*/
            lprob += R::dlnorm(sub_pars[1], -4.117028,0.4751615, 1);
            /*correct for the prior from serology season (lognormal):"65+" lm=-2.977965, ls=1.331832*/
            lprob += R::dlnorm(sub_pars[2], -2.977965,1.331832, 1);
        }

        return lprob;
    };

    auto llikelihood_function = [&]( const Eigen::VectorXd &pars,
            const Eigen::MatrixXd &contact_regular )
    {
        std::vector<Eigen::MatrixXd> week_result;
        for ( size_t i = 0; i < (size_t)no_strains; ++i )
        {
            auto sub_pars = pars.segment( i*8, 8 );

            auto init_inf = Eigen::VectorXd::Constant( 
                    nag, pow(10,sub_pars[7]) );

            Eigen::VectorXd susc(7);
            susc << sub_pars[4], sub_pars[4], sub_pars[4],
                 sub_pars[5], sub_pars[5], sub_pars[5],
                 sub_pars[6];

            auto seed_vec = flu::data::separate_into_risk_groups( 
                    init_inf, risk_proportions  );
                
            Eigen::VectorXd resist_vec = Eigen::VectorXd::Zero(seed_vec.col());

            auto result = infectionODE(pop_vec, 
                    seed_vec,
                    resist_vec,
                    time_latent, time_infectious, 
                    susc,
                    contact_regular, sub_pars[3], 
                    vaccine_calendars[i], 7*24 );
            auto week = days_to_weeks_5AG( result );
            week_result.push_back(week);
        }

        auto lprob = 0.0;
        for (int w = 0; w < week_result[0].rows(); ++w) {
            for (int ag = 0; ag < week_result[0].cols(); ++ag) {
                std::vector<double> e_ps;
                double sum_e_ps = 0.0;
                for (int st = 0; st < no_strains; ++st) {
                    Eigen::VectorXd eps(5);
                    eps << pars[st*8], pars[st*8],
                        pars[st*8+1], pars[st*8+1],
                        pars[st*8+2];
                    e_ps.push_back(eps[ag]*week_result[st](w,ag)
                            /pop_RCGP[ag]);
                    sum_e_ps += e_ps[st];
                }
                sum_e_ps *= 1+pars[no_strains*8];
                if (sum_e_ps > 1)
                    lprob += -1e10;
                else {
                    lprob += R::dbinom(ili(w,ag), mon_pop(w,ag), sum_e_ps, 1);
                    for (int st = 0; st < no_strains; ++st)
                    {
                        lprob += R::dbinom(positives[st](w,ag), n_samples(w,ag), e_ps[st]/sum_e_ps, 1);
                    }
                }
            }
        }
        return lprob;
    };

    auto curr_lprior = lprior_function(curr_parameters);

    auto curr_llikelihood = llikelihood_function( curr_parameters,
            current_contact_regular );

    auto proposal_state = proposal::initialize( curr_parameters.size() );

    /**************************************************************************************************************************************************************
    Start of the MCMC
    **************************************************************************************************************************************************************/

    step_mat=1;            /*number of contacts exchanged*/
    double p_ac_mat=0.10;          /*prob to redraw matrices*/

    size_t sampleCount = 0;
    int k = 0;
    while(sampleCount<nbatch)
    {
        ++k;

        /*update of the variance-covariance matrix and the mean vector*/
        proposal_state = proposal::update( std::move( proposal_state ),
                curr_parameters, k );

        /*
        auto prop_parameters = proposal::haario_adapt_scale(
                curr_parameters,
                proposal_state.chol_emp_cov,
                proposal_state.chol_ini,0.05, 
                proposal_state.adaptive_scaling );
        */
        /*
        auto epsilon = 0.001;
        if (k>10000)
            epsilon = 0;
        auto prop_parameters = proposal::haario( k,
                curr_parameters,
                proposal_state.chol_emp_cov, epsilon );*/

        auto prop_parameters = proposal::sherlock( k,
                curr_parameters,
                proposal_state );

        auto prop_lprior = lprior_function(prop_parameters);
        auto prior_ratio = prop_lprior - curr_lprior;

        if (!std::isfinite(prior_ratio))
        {
            //Rcpp::Rcout << "Invalid proposed par" << std::endl;
            // TODO: code duplication with failure of acceptance
            proposal_state = proposal::accepted( 
                    std::move(proposal_state), false, k );
        } else {
            auto prop_c = curr_c;

            /*do swap of contacts step_mat times (reduce or increase to change 'distance' of new matrix from current)*/
            // TODO/WARN Need to draw this before hand and pass it as data to
            // likelihood function... Even when doing that we still need to know k,
            // so might as well make the likelihood function increase k when called
            if(R::runif(0,1) < p_ac_mat)
                prop_c = contacts::bootstrap_contacts( std::move(prop_c),
                        polymod, step_mat );

            auto prop_contact_regular = 
                contacts::to_symmetric_matrix( prop_c, age_data );

            /*computes the associated likelihood with the proposed values*/
            auto prop_llikelihood = llikelihood_function( prop_parameters,
                   prop_contact_regular );


            /*Acceptance rate include the likelihood and the prior but no correction for the proposal as we use a symmetrical RW*/
            // Make sure accept works with -inf prior
            // MCMC-R alternative prior?
            if (std::isinf(prop_llikelihood) && std::isinf(curr_llikelihood) )
                my_acceptance_rate = exp(prior_ratio); // We want to explore and find a non infinite likelihood
            else 
                my_acceptance_rate=
                    exp(prop_llikelihood-curr_llikelihood+
                    prior_ratio);

            if(R::runif(0,1)<my_acceptance_rate &&
                     std::isfinite(my_acceptance_rate)) /*with prior*/
            {
                /*update the acceptance rate*/
                proposal_state = proposal::accepted( 
                        std::move(proposal_state), true, k );

                curr_parameters = prop_parameters;

                /*update current likelihood*/
                curr_llikelihood=prop_llikelihood;
                curr_lprior = prop_lprior;

                /*new proposed contact matrix*/
                /*update*/
                curr_c = prop_c;
                current_contact_regular=prop_contact_regular;
            }
            else /*if reject*/
            {
                proposal_state = proposal::accepted( 
                        std::move(proposal_state), false, k );
            }
        }

        if(k%blen==0 && k>=(int)nburn)
        {
            // Add results
            results.llikelihoods[sampleCount] = curr_llikelihood;
            results.batch.row( sampleCount ) = curr_parameters;
            for( size_t i = 0; i < curr_c.contacts.size(); ++i )
                results.contact_ids( sampleCount, i ) =
                    curr_c.contacts[i].id;

            ++sampleCount;
        }
    }
    return results;
}
