#include "adios2Routines.h"
#include "couplingTypes.h"
#include "dataprocess.h"
#include "importpart3mesh.h"
#include "commpart1.h"
#include "BoundaryDescr3D.h"
#include "testutilities.h"
#include <string>
#include <fstream> 

void exParFor() {
  Kokkos::parallel_for(
      4, KOKKOS_LAMBDA(const int i) {
        printf("Hello from kokkos thread i = %i\n", i);
      });
}

void finalize(){
  std::cerr << " before kokkos finalize\n";
  Kokkos::finalize();
  std::cerr << " done kokkos finalize\n";
  MPI_Finalize();
}

int main(int argc, char **argv){
  int rank;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  Kokkos::initialize(argc, argv);
  if(!rank) {
    printf("Hello World on Kokkos execution space %s\n",
         typeid(Kokkos::DefaultExecutionSpace).name());
    exParFor();
  }
  if(argc != 2) {
    if(!rank) printf("Usage: %s <0:reader !0:writer>\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  const int whoami = atoi(argv[1]);

  adios2::ADIOS adios(MPI_COMM_WORLD, adios2::DebugON);

  if(whoami==0) {
    coupler::adios2_handler reader(adios,"reader");
    reader.IO.SetEngine("Sst");
    reader.eng = reader.IO.Open("foo.bp", adios2::Mode::Read);
    finalize();
    return 0;
  } else {
    coupler::adios2_handler writer(adios,"writer");
    writer.IO.SetEngine("Sst");
    writer.eng = writer.IO.Open("foo.bp", adios2::Mode::Write);
    finalize();
    return 0;
  }
  return 0;
}

