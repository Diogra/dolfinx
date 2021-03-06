import numpy
from mpi4py import MPI

from dolfinx import Mesh, geometry, cpp
from dolfinx.cpp.mesh import CellType
from dolfinx.geometry import BoundingBoxTree
from dolfinx_utils.test.skips import skip_in_parallel


@skip_in_parallel
def test_manifold_point_search():
    # Simple two-triangle surface in 3d
    vertices = [(0.0, 0.0, 1.0), (1.0, 1.0, 1.0), (1.0, 0.0, 0.0), (0.0, 1.0,
                                                                    0.0)]
    cells = [(0, 1, 2), (0, 1, 3)]
    mesh = Mesh(MPI.COMM_WORLD, CellType.triangle,
                numpy.array(vertices, dtype=numpy.float64),
                numpy.array(cells, dtype=numpy.int32), [])

    bb = BoundingBoxTree(mesh, mesh.topology.dim)
    p = numpy.array([0.5, 0.25, 0.75])
    cell_candidates = geometry.compute_collisions_point(bb, p)
    cell = cpp.geometry.select_colliding_cells(mesh, cell_candidates, p, 1)
    assert cell[0] == 0

    p = numpy.array([0.25, 0.5, 0.75])
    cell_candidates = geometry.compute_collisions_point(bb, p)
    cell = cpp.geometry.select_colliding_cells(mesh, cell_candidates, p, 1)
    assert cell[0] == 1
