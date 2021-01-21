/*
 * Copyright (c), University of Bologna and ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *			* Redistributions of source code must retain the above copyright notice, this
 *				list of conditions and the following disclaimer.
 *
 *			* Redistributions in binary form must reproduce the above copyright notice,
 *				this list of conditions and the following disclaimer in the documentation
 *				and/or other materials provided with the distribution.
 *
 *			* Neither the name of the copyright holder nor the names of its
 *				contributors may be used to endorse or promote products derived from
 *				this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Daniele Cesarini, University of Bologna
*/

#include "cntd.h"

void print_report()
{
	int i, j;
	int world_rank, world_size;
    
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	
	char hostnames_world[world_size][STRING_SIZE];
	uint64_t energy_pkg_world[world_size];
	uint64_t energy_dram_world[world_size];

	char host[STRING_SIZE];
	gethostname(host, sizeof(host));

    uint64_t energy_pkg_tot = 0;
    uint64_t energy_dram_tot = 0;
    for(i = 0; i < cntd->num_sockets; i++)
    {
        energy_pkg_tot += cntd->energy_pkg[i];
        energy_dram_tot += cntd->energy_dram[i];
    }

	PMPI_Gather(host, STRING_SIZE, MPI_CHAR, 
        hostnames_world, STRING_SIZE, MPI_CHAR, 
		0, MPI_COMM_WORLD);
	PMPI_Gather(&energy_pkg_tot, 1, MPI_UNSIGNED_LONG, 
		energy_pkg_world, 1, MPI_UNSIGNED_LONG, 
		0, MPI_COMM_WORLD);
	PMPI_Gather(&energy_dram_tot, 1, MPI_UNSIGNED_LONG, 
		energy_dram_world, 1, MPI_UNSIGNED_LONG, 
		0, MPI_COMM_WORLD);

    uint64_t energy_pkg_sampling_world[world_size][cntd->sampling_cnt[CURR]];
    uint64_t energy_dram_sampling_world[world_size][cntd->sampling_cnt[CURR]];
    if(cntd->sampling_report)
    {
        PMPI_Gather(cntd->energy_pkg_sampling, cntd->sampling_cnt[CURR], MPI_UNSIGNED_LONG, 
		    energy_pkg_sampling_world, cntd->sampling_cnt[CURR], MPI_UNSIGNED_LONG, 
		    0, MPI_COMM_WORLD);
        PMPI_Gather(cntd->energy_dram_sampling, cntd->sampling_cnt[CURR], MPI_UNSIGNED_LONG, 
		    energy_dram_sampling_world, cntd->sampling_cnt[CURR], MPI_UNSIGNED_LONG, 
		    0, MPI_COMM_WORLD);
    }

	if(world_rank == 0)
	{
		int flag = FALSE;
		uint64_t tot_energy_pkg_uj = 0;
		uint64_t tot_energy_dram_uj = 0;
		double tot_energy_pkg, tot_energy_dram;
        int hosts_idx[world_size];
		int hosts_count = 0;

		double exe_time = cntd->exe_time[END] - cntd->exe_time[START];
		for(i = 0; i < world_size; i++)
		{
            for(j = 0; j < hosts_count; j++)
            {
                if(strncmp(hostnames_world[hosts_idx[j]], hostnames_world[i], STRING_SIZE) == 0)
                {
                    flag = TRUE;
                    break;
                }
            }
            if(flag == FALSE)
            {
                hosts_idx[hosts_count] = i;
                hosts_count++;
                tot_energy_pkg_uj += energy_pkg_world[i];
                tot_energy_dram_uj += energy_dram_world[i];
            }
            else
                flag = FALSE;
		}
		tot_energy_pkg = ((double) tot_energy_pkg_uj) / 1.0E6;
		tot_energy_dram = ((double) tot_energy_dram_uj) / 1.0E6;

        if(cntd->sampling_report)
        {
            FILE *report_fd = fopen("report.csv", "w");
            if(report_fd == NULL)
            {
                fprintf(stderr, "Error: <countdown> Failed to create report!");
		        PMPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }

            // Write label
            fprintf(report_fd, "sample_duration");
            for(j = 0; j < hosts_count; j++)
                fprintf(report_fd, 
                ";%s--energy_pkg;%s--energy_dram;%s--energy_tot;%s--power_pkg;%s--power_dram;%s--power_tot", 
                hostnames_world[hosts_idx[j]],
                hostnames_world[hosts_idx[j]], 
                hostnames_world[hosts_idx[j]], 
                hostnames_world[hosts_idx[j]], 
                hostnames_world[hosts_idx[j]], 
                hostnames_world[hosts_idx[j]]);
            fprintf(report_fd, "\n");

            for(i = 0; i < cntd->sampling_cnt[CURR]; i++)
            {
                fprintf(report_fd, "%.3f", cntd->sampling[i]);
                for(j = 0; j < hosts_count; j++)
                {
                    fprintf(report_fd, ";%.2f;%.2f;%.2f;%.2f;%.2f;%.2f", 
                        energy_pkg_sampling_world[hosts_idx[j]][i] / 1.0E6,
                        energy_dram_sampling_world[hosts_idx[j]][i] / 1.0E6,
                        (energy_pkg_sampling_world[hosts_idx[j]][i] + energy_dram_sampling_world[hosts_idx[j]][i]) / 1.0E6,
                        energy_pkg_sampling_world[hosts_idx[j]][i] / 1.0E6 / cntd->sampling[i],
                        energy_dram_sampling_world[hosts_idx[j]][i] / 1.0E6 / cntd->sampling[i],
                        (energy_pkg_sampling_world[hosts_idx[j]][i] + energy_dram_sampling_world[hosts_idx[j]][i]) / 1.0E6 / cntd->sampling[i]);
                }
                fprintf(report_fd, "\n");
            }   

            fclose(report_fd);
        }

		printf("#####################################\n");
		printf("############# COUNTDOWN #############\n");
		printf("#####################################\n");
		printf("Execution time: %.3f sec\n", exe_time);
		printf("############### ENERGY ##############\n");
		printf("Package energy: %.3f J\n", tot_energy_pkg);
		printf("DRAM energy: %.3f J\n", tot_energy_dram);
		printf("Total energy: %.3f J\n", tot_energy_pkg + tot_energy_dram);
		printf("############# AVG POWER #############\n");
		printf("AVG Package power: %.2f W\n", tot_energy_pkg / exe_time);
		printf("AVG DRAM power: %.2f W\n", tot_energy_dram  / exe_time);
		printf("AVG power: %.2f W\n", (tot_energy_pkg + tot_energy_dram) / exe_time);
		printf("#####################################\n");
	}
}