#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mpi.h>

#include "core_allvars.h"
#include "core_proto.h"


char bufz0[1000];
int exitfail = 1;

struct sigaction saveaction_XCPU;
volatile sig_atomic_t gotXCPU = 0;



void termination_handler(int signum)
{
  gotXCPU = 1;
  sigaction(SIGXCPU, &saveaction_XCPU, NULL);
  if(saveaction_XCPU.sa_handler != NULL)
    (*saveaction_XCPU.sa_handler) (signum);
}



void myexit(int signum)
{
  printf("Task: %d\tnode: %s\tis exiting.\n\n\n", ThisTask, ThisNode);
  exit(signum);
}



void bye()
{
  MPI_Finalize();
  free(ThisNode);

  if(exitfail)
  {
    unlink(bufz0);

    if(ThisTask == 0 && gotXCPU == 1)
      printf("Received XCPU, exiting. But we'll be back.\n");
  }
}



int main(int argc, char **argv)
{
  int filenr, tree, halonr;
  struct sigaction current_XCPU;

  struct stat filestatus;
  FILE *fd;
  time_t start, current;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
  MPI_Comm_size(MPI_COMM_WORLD, &NTask);

  ThisNode = malloc(MPI_MAX_PROCESSOR_NAME * sizeof(char));

  MPI_Get_processor_name(ThisNode, &nodeNameLen);
  if (nodeNameLen >= MPI_MAX_PROCESSOR_NAME) 
  {
    printf("Node name string not long enough!...\n");
    ABORT(701);
  }

  if(argc != 2)
  {
    printf("\n  usage: L-Galaxies <parameterfile>\n\n");
    ABORT(1);
  }

  atexit(bye);

  sigaction(SIGXCPU, NULL, &saveaction_XCPU);
  current_XCPU = saveaction_XCPU;
  current_XCPU.sa_handler = termination_handler;
  sigaction(SIGXCPU, &current_XCPU, NULL);

  read_parameter_file(argv[1]);
  init();

  /* a small delay so that processors dont use the same file */
  time(&start);
  do
    time(&current);
  while(difftime(current, start) < 5.0 * ThisTask);

  for(filenr = FirstFile; filenr <= LastFile; filenr++)
  {
    sprintf(bufz0, "%s/%s_z%1.3f_%d", OutputDir, FileNameGalaxies, ZZ[ListOutputSnaps[0]], filenr);
    if(stat(bufz0, &filestatus) == 0)	 // seems to exist 
      continue;

    if((fd = fopen(bufz0, "w")))
      fclose(fd);

    load_tree_table(filenr);

    for(tree = 0; tree < Ntrees; tree++)
    {
      
      if(gotXCPU)
        ABORT(5);

      if(tree % 10000 == 0)
      {
        printf("\ttask: %d\tnode: %s\tfile: %i\ttree: %i of %i\n", ThisTask, ThisNode, filenr, tree, Ntrees);
        fflush(stdout);
      }

      load_tree(filenr, tree);

      gsl_rng_set(random_generator, filenr * 100000 + tree);
      NumGals = 0;
      GalaxyCounter = 0;
      for(halonr = 0; halonr < TreeNHalos[tree]; halonr++)
        if(HaloAux[halonr].DoneFlag == 0)
        construct_galaxies(halonr);

      save_galaxies(filenr, tree);
      free_galaxies_and_tree();
    }

    finalize_galaxy_file(filenr);
    free_tree_table();

    printf("\ndone file %d\n\n", filenr);
  }

  exitfail = 0;
  return 0;
}
