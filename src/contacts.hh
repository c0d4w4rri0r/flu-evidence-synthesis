#ifndef FLU_CONTACTS_HH
#define FLU_CONTACTS_HH

#include<array>
#include<vector>
#include<string>

#include<state.hh>
#include<data.hh>

namespace flu {

    namespace contacts {
        /// Create a contact, holding its age, and contacts (we, N1, N2, etc)..
        struct contact_t
        {
            size_t id;
            int age, we, N1, N2, N3, N4, N5, N6, N7, AG;
        };

        /// Struct to keep all contacts and some metadata
        struct contacts_t
        {
            std::array<contact_t,POLY_PART> contacts;

            /// Total number of participants of each age group
            int ni[90];

            /// Total amount of contact data for weekend
            size_t nwe;
        };

        contacts_t load_contacts( const std::string &path );

        /**
         * \brief Shuffle given contacts according to id. Assumes the given
         * contacts are already sorted
         */
        contacts_t shuffle_by_id( const contacts_t &sorted_c, const std::vector<size_t> &ids );

        std::vector<double> to_symmetric_matrix( 
                const contacts_t &contacts, 
                const data::age_data_t &age_data );
    };

};
#endif
