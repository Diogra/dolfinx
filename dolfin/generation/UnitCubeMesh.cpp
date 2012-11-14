// Copyright (C) 2005-2008 Anders Logg
//
// This file is part of DOLFIN.
//
// DOLFIN is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DOLFIN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.
//
// Modified by Garth N. Wells, 2007.
//
// First added:  2005-12-02
// Last changed: 2010-10-19

#include <dolfin/common/MPI.h>
#include <dolfin/common/Timer.h>
#include <dolfin/mesh/MeshPartitioning.h>
#include <dolfin/mesh/MeshEditor.h>
#include "UnitCubeMesh.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
UnitCubeMesh::UnitCubeMesh(uint nx, uint ny, uint nz) : Mesh()
{
  Timer timer("generate unit cube mesh");

  // Receive mesh according to parallel policy
  if (MPI::is_receiver())
  {
    MeshPartitioning::build_distributed_mesh(*this);
    return;
  }

  // Check input
  if ( nx < 1 || ny < 1 || nz < 1 )
  {
    dolfin_error("UnitCubeMesh.cpp",
                 "create unit cube",
                 "Cube has non-positive number of vertices in some dimension: number of vertices must be at least 1 in each dimension");
  }

  // Set name
  rename("mesh", "Mesh of the unit cube (0,1) x (0,1) x (0,1)");

  // Open mesh for editing
  MeshEditor editor;
  editor.open(*this, CellType::tetrahedron, 3, 3);

  // Storage for vertex coordinates
  std::vector<double> x(3);

  // Create vertices
  editor.init_vertices((nx+1)*(ny+1)*(nz+1));
  uint vertex = 0;
  for (uint iz = 0; iz <= nz; iz++)
  {
    x[2] = static_cast<double>(iz) / static_cast<double>(nz);
    for (uint iy = 0; iy <= ny; iy++)
    {
      x[1] = static_cast<double>(iy) / static_cast<double>(ny);
      for (uint ix = 0; ix <= nx; ix++)
      {
        x[0] = static_cast<double>(ix) / static_cast<double>(nx);
        editor.add_vertex(vertex, x);
        vertex++;
      }
    }
  }


  // Create tetrahedra
  editor.init_cells(6*nx*ny*nz);
  uint cell = 0;
  std::vector<uint> cells(4);
  for (uint iz = 0; iz < nz; iz++)
  {
    for (uint iy = 0; iy < ny; iy++)
    {
      for (uint ix = 0; ix < nx; ix++)
      {
        const uint v0 = iz*(nx + 1)*(ny + 1) + iy*(nx + 1) + ix;
        const uint v1 = v0 + 1;
        const uint v2 = v0 + (nx + 1);
        const uint v3 = v1 + (nx + 1);
        const uint v4 = v0 + (nx + 1)*(ny + 1);
        const uint v5 = v1 + (nx + 1)*(ny + 1);
        const uint v6 = v2 + (nx + 1)*(ny + 1);
        const uint v7 = v3 + (nx + 1)*(ny + 1);

        // Note that v0 < v1 < v2 < v3 < vmid.
        cells[0] = v0; cells[1] = v1; cells[2] = v3; cells[3] = v7;
        editor.add_cell(cell++, cells);

        cells[0] = v0; cells[1] = v1; cells[2] = v7; cells[3] = v5;
        editor.add_cell(cell++, cells);

        cells[0] = v0; cells[1] = v5; cells[2] = v7; cells[3] = v4;
        editor.add_cell(cell++, cells);

        cells[0] = v0; cells[1] = v3; cells[2] = v2; cells[3] = v7;
        editor.add_cell(cell++, cells);

        cells[0] = v0; cells[1] = v6; cells[2] = v4; cells[3] = v7;
        editor.add_cell(cell++, cells);

        cells[0] = v0; cells[1] = v2; cells[2] = v6; cells[3] = v7;
        editor.add_cell(cell++, cells);
      }
    }
  }

  // Close mesh editor
  editor.close();

  // Broadcast mesh according to parallel policy
  if (MPI::is_broadcaster())
    MeshPartitioning::build_distributed_mesh(*this);
}
//-----------------------------------------------------------------------------