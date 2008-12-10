// Copyright (C) 2007 Magnus Vikstrom.
// Licensed under the GNU LGPL Version 2.1.
//
// Modified by Anders Logg, 2008.
// Modified by Ola Skavhaug, 2008.
// Modified by Garth N. Wells, 2008.
// Modified by Niclas Jansson, 2008.
//
// First added:  2007-04-03
// Last changed: 2008-12-09

#include <dolfin/log/log.h>
#include <dolfin/main/MPI.h>
#include <dolfin/graph/Graph.h>
#include <dolfin/graph/GraphBuilder.h>
#include <dolfin/graph/GraphPartition.h>
#include <dolfin/parameter/parameters.h>
#include <dolfin/mesh/LocalMeshData.h>
#include "Cell.h"
#include "Facet.h"
#include "Vertex.h"
#include "MeshFunction.h"
#include "MeshPartitioning.h"

#if defined HAS_PARMETIS && HAS_MPI

#include <parmetis.h>
#include <mpi.h>

using namespace dolfin;

//-----------------------------------------------------------------------------
void MeshPartitioning::partition(Mesh& mesh, LocalMeshData& data)
{
  dolfin_debug("Partitioning mesh...");
  
  // Get number of processes and process number
  const uint num_processes = MPI::num_processes();
  const uint process_number = MPI::process_number();

  // Get dimensions of local data
  const uint num_local_cells = data.cell_vertices().size();
  const uint num_local_vertices = data.vertex_coordinates().size();
  const uint num_cell_vertices = data.cell_vertices()[0].size();
  dolfin_debug1("num_local_cells = %d", num_local_cells);

  // Communicate number of cells between all processors
  std::vector<uint> num_cells(num_processes);
  num_cells[process_number] = num_local_cells;
  MPI::gather(num_cells);

  // Build elmdist array with cell offsets for all processors
  int* elmdist = new int[num_processes + 1];
  elmdist[0] = 0;
  for (uint i = 1; i < num_processes + 1; ++i)
    elmdist[i] = elmdist[i - 1] + num_cells[i - 1];

  // Build eptr and eind arrays storing cell-vertex connectivity
  int* eptr = new int[num_local_cells + 1];
  int* eind = new int[num_local_cells * num_cell_vertices];
  for (uint i = 0; i < num_local_cells; i++)
  {
    dolfin_assert(data.cell_vertices()[i].size() == num_cell_vertices);
    eptr[i] = i * num_cell_vertices;
    for (uint j = 0; j < num_cell_vertices; j++)
      eind[eptr[i] + j] = data.cell_vertices()[i][j];
  }
  eptr[num_local_cells] = num_local_cells * num_cell_vertices;

  // Number of nodes shared for dual graph (partition along facets)
  int ncommonnodes = num_cell_vertices - 1;

  // Number of partitions (one for each process)
  int nparts = num_processes;

  // Vertex weights
  float* tpwgts = new float[num_processes];
  for (uint p = 0; p < num_processes; p++)
    tpwgts[p] = 1.0 / static_cast<float>(nparts);

  // Partitioning array for vertices to be computed by ParMETIS
  int* part = new int[num_local_vertices];

  // Prepare remaining arguments for ParMETIS
  int* elmwgt = 0;
  int wgtflag = 0;
  int numflag = 0;
  int ncon    = 1;
  float ubvec = 1.05;
  int options = 0;
  int edgecut = 0;

  // FIXME: Move this part to MPI wrapper
  MPI_Comm comm;
  MPI_Comm_dup(MPI_COMM_WORLD, &comm);
  
  // Call ParMETIS to partition mesh
  ParMETIS_V3_PartMeshKway(elmdist, eptr, eind,
                           elmwgt, &wgtflag, &numflag, &ncon,
                           &ncommonnodes, &nparts,
                           tpwgts, &ubvec, &options,
                           &edgecut, part, &comm);
  message("Partitioned mesh, edge cut is %d.", edgecut);

  // Cleanup
  delete [] elmdist;
  delete [] eptr;
  delete [] eind;
  delete [] tpwgts;
  delete [] part;
}
//-----------------------------------------------------------------------------
void MeshPartitioning::partition_vertices(const LocalMeshData& data,
                                          std::vector<uint>& vertex_partition)
{
  // This function computes a (new) partition of all vertices by
  // computing an array vertex_partition that assigns a new process
  // number to each vertex stored by the local process.

  dolfin_debug("Computing geometric partitioning of vertices...");

  // Get number of processes and process number
  const uint num_processes = MPI::num_processes();
  const uint process_number = MPI::process_number();

  // Get dimensions of local data
  const uint num_local_vertices = data.vertex_coordinates().size();
  const uint gdim = data.vertex_coordinates()[0].size();
  dolfin_assert(num_local_vertices > 0);
  dolfin_assert(gdim > 0);

  // FIXME: Why is this necessary?
  // Duplicate MPI communicator
  MPI_Comm comm;
  MPI_Comm_dup(MPI_COMM_WORLD, &comm);

  // Communicate number of vertices between all processors
  int* vtxdist = new int[num_processes + 1];
  vtxdist[process_number] = static_cast<int>(num_local_vertices);
  dolfin_debug("Communicating vertex distribution across processors...");
  MPI_Allgather(&vtxdist[process_number], 1, MPI_INT, 
                 vtxdist,                 1, MPI_INT, MPI_COMM_WORLD);

  // Build vtxdist array with vertex offsets for all processor
  int sum = vtxdist[0];
  vtxdist[0] = 0;
  for (uint i = 1; i < num_processes + 1; ++i)
  {
    const int tmp = vtxdist[i];
    vtxdist[i] = sum;
    sum += tmp;
  }

  // Prepare arguments for ParMetis
  int ndims = static_cast<int>(gdim);
  int* part = new int[num_local_vertices];
  float* xyz = new float[gdim*num_local_vertices];
  for (uint i = 0; i < num_local_vertices; ++i)
    for (uint j = 0; j < gdim; ++j)
      xyz[i*gdim + j] = data.vertex_coordinates()[i][j];

  // Call ParMETIS to partition vertex distribution array
  dolfin_debug("Calling ParMETIS to distribute vertices");
  ParMETIS_V3_PartGeom(vtxdist, &ndims, xyz, part, &comm);
  dolfin_debug("Done calling ParMETIS to distribute vertices");

  // Copy partition data
  vertex_partition.clear();
  vertex_partition.reserve(num_local_vertices);
  for (uint i = 0; i < num_local_vertices; i++)
    vertex_partition.push_back(static_cast<uint>(part[i]));

  // Cleanup
  delete [] vtxdist;
  delete [] part;
  delete [] xyz;
}
//-----------------------------------------------------------------------------
void MeshPartitioning::distribute_vertices(LocalMeshData& data,
                                           const std::vector<uint>& vertex_partition)
{
  // This function redistributes the vertices stored by each process
  // according to the array vertex_partition.

  dolfin_debug("Distributing local mesh data according to vertex partition...");

  // Use MPI::distribute() here
  
}
//-----------------------------------------------------------------------------
void MeshPartitioning::partition_cells()
{
  dolfin_debug("Computing topological partitioning of cells...");  
}
//-----------------------------------------------------------------------------

#else

//-----------------------------------------------------------------------------
void MeshPartitioning::partition(Mesh& mesh, LocalMeshData& data)
{
  error("Mesh partitioning requires MPI and ParMETIS.");
}
//-----------------------------------------------------------------------------
void MeshPartitioning::partition_vertices(const LocalMeshData& data,
                                          int* part)
{
  error("Mesh partitioning requires MPI and ParMETIS.");
}
//-----------------------------------------------------------------------------
void MeshPartitioning::distribute_vertices(LocalMeshData& data,
                                           const int* part)
{
  error("Mesh partitioning requires MPI and ParMETIS.");
}
//-----------------------------------------------------------------------------
void MeshPartitioning::partition_cells()
{
  error("Mesh partitioning requires MPI and ParMETIS.");
}
//-----------------------------------------------------------------------------

#endif
