#include "contacts.hh"

#include <stdio.h>

#include "io.hh"
#include "state.hh"

namespace flu
{
    std::vector<double> load_contact_regular( const std::string& contacts_filepath, const state_t &state, int *age_sizes, int *AG_sizes, const contacts_t &c )
    {
        int nc, age_part, AG_part;
        double ww[POLY_PART], mij[49], w_norm[7], cij[49], cij_pro;
        int c_age[POLY_PART], c_we[POLY_PART], c_N1[POLY_PART], c_N2[POLY_PART], c_N3[POLY_PART], c_N4[POLY_PART], c_N5[POLY_PART], c_N6[POLY_PART], c_N7[POLY_PART], c_AG[POLY_PART], c_ni[90];
        int curr_age[POLY_PART], curr_we[POLY_PART], curr_N1[POLY_PART], curr_N2[POLY_PART], curr_N3[POLY_PART], curr_N4[POLY_PART], curr_N5[POLY_PART], curr_N6[POLY_PART], curr_N7[POLY_PART], curr_AG[POLY_PART], curr_ni[90], curr_nwe;
        FILE *contacts_PM;
        int c_nwe = 0;
        contacts_PM=read_file( contacts_filepath );

        for(size_t i=0; i<POLY_PART; i++)
        {
            save_fscanf(contacts_PM,"%d %d %d %d %d %d %d %d %d", &c_age[i], &c_we[i], &c_N1[i], &c_N2[i], &c_N3[i], &c_N4[i], &c_N5[i], &c_N6[i], &c_N7[i]);
            age_part=c_age[i];
            c_ni[age_part]++;
            if(c_we[i]>0) c_nwe++;
            c_AG[i]=0;
            if(age_part>0) c_AG[i]++;
            if(age_part>4) c_AG[i]++;
            if(age_part>14) c_AG[i]++;
            if(age_part>24) c_AG[i]++;
            if(age_part>44) c_AG[i]++;
            if(age_part>64) c_AG[i]++;
        }


        for(size_t i=0;i<90;i++)
            curr_ni[i]=0;
        curr_nwe=0;
        for(size_t i=0; i<POLY_PART; i++)
        {
            nc = state.number_contacts[i];

            age_part=c_age[nc];
            curr_ni[age_part]++;
            if(c_we[nc]>0) curr_nwe++;

            curr_age[i]=c_age[nc];
            curr_AG[i]=c_AG[nc];
            curr_we[i]=c_we[nc];
            curr_N1[i]=c_N1[nc];
            curr_N2[i]=c_N2[nc];
            curr_N3[i]=c_N3[nc];
            curr_N4[i]=c_N4[nc];
            curr_N5[i]=c_N5[nc];
            curr_N6[i]=c_N6[nc];
            curr_N7[i]=c_N7[nc];
        }
        /*update of the weights*/
        for(size_t i=0;i<7;i++)
            w_norm[i]=0;

        for(size_t i=0;i<49;i++)
            mij[i]=0;

        for(size_t i=0; i<POLY_PART; i++)
        {
            age_part=c.contacts[i].age;
            AG_part=c.contacts[i].AG;
            if(c.contacts[i].we==0)
                ww[i]=(double)age_sizes[age_part]/c.ni[age_part]*5/(POLY_PART-c.nwe);
            else
                ww[i]=(double)age_sizes[age_part]/c.ni[age_part]*2/c.nwe;

            w_norm[AG_part]+=ww[i];
            mij[7*AG_part]+=c.contacts[i].N1*ww[i];
            mij[7*AG_part+1]+=c.contacts[i].N2*ww[i];
            mij[7*AG_part+2]+=c.contacts[i].N3*ww[i];
            mij[7*AG_part+3]+=c.contacts[i].N4*ww[i];
            mij[7*AG_part+4]+=c.contacts[i].N5*ww[i];
            mij[7*AG_part+5]+=c.contacts[i].N6*ww[i];
            mij[7*AG_part+6]+=c.contacts[i].N7*ww[i];
        }

        /*Compute the contact matrix*/
        for(size_t i=0; i<49; i++)
        {
            if(w_norm[i/7]>0)
                mij[i]/=w_norm[i/7];
            cij[i]=mij[i]/AG_sizes[i%7];
        }

        std::vector<double> contact_regular( NAG2 );
        for(size_t i=0; i<7; i++)
        {
            contact_regular[i*7+i]=cij[i*7+i];
            for(size_t j=0;j<i;j++)
            {
                cij_pro=(cij[i*7+j]+cij[j*7+i])/2;
                contact_regular[i*7+j]=cij_pro;
                contact_regular[j*7+i]=cij_pro;
            }
        }

        fclose( contacts_PM );

        return contact_regular;
    }
};
