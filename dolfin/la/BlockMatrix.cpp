// Copyright (C) 2008 Kent-Andre Mardal.
// Licensed under the GNU LGPL Version 2.1.
//
// Modified by Anders Logg, 2008.
// Modified by Garth N. Wells, 2011.
//
// First added:  2008-08-25
// Last changed: 2011-01-13

#include <iostream>
#include <boost/scoped_ptr.hpp>

#include "dolfin/common/utils.h"
#include "BlockVector.h"
#include "DefaultFactory.h"
#include "GenericVector.h"
#include "Matrix.h"
#include "BlockMatrix.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
BlockMatrix::BlockMatrix(uint m, uint n)
    : matrices(m, std::vector<boost::shared_ptr<GenericMatrix> >(n))
{
  for (uint i = 0; i < m; i++)
    for (uint j = 0; j < n; j++)
      matrices[i][j].reset(new Matrix());
}
//-----------------------------------------------------------------------------
BlockMatrix::~BlockMatrix()
{
  // Do nothing
}
//-----------------------------------------------------------------------------
void BlockMatrix::set(uint i, uint j, GenericMatrix& m)
{
  error("BlockMatrix::set needs revision.");
  //matrices[i*n+j] = m.copy(); //FIXME. not obvious that copy is the right thing
  //matrices[i*n+j] = &m;         //FIXME. not obvious that copy is the right thing
}
//-----------------------------------------------------------------------------
const GenericMatrix& BlockMatrix::get(uint i, uint j) const
{
  return *(matrices[i][j]);
}
//-----------------------------------------------------------------------------
GenericMatrix& BlockMatrix::get(uint i, uint j)
{
  return *(matrices[i][j]);
}
//-----------------------------------------------------------------------------
dolfin::uint BlockMatrix::size(uint dim) const
{
  if (dim == 0)
    return matrices.size();
  else if (dim == 1)
    return matrices[0].size();
  else
    error("BlockMatrix has rank 2!");

  return 0;
}
//-----------------------------------------------------------------------------
void BlockMatrix::zero()
{
  for(uint i = 0; i < matrices.size(); i++)
    for(uint j = 0; j < matrices[i].size(); j++)
      this->get(i, j).zero();
}
//-----------------------------------------------------------------------------
void BlockMatrix::apply(std::string mode)
{
  for(uint i = 0; i < matrices.size(); i++)
    for(uint j = 0; j < matrices[i].size(); j++)
      this->get(i,j).apply(mode);
}
//-----------------------------------------------------------------------------
std::string BlockMatrix::str(bool verbose) const
{
  std::stringstream s;

  if (verbose)
  {
    s << str(false) << std::endl << std::endl;
    for (uint i = 0; i < matrices.size(); i++)
    {
      for (uint j = 0; i < matrices[0].size(); j++)
      {
        s << "  BlockMatrix (" << i << ", " << j << ")" << std::endl << std::endl;
        s << indent(indent(get(i, j).str(true))) << std::endl;
      }
    }
  }
  else
    s << "<BlockMatrix containing " << matrices.size() << " x " << matrices[0].size() << " blocks>";

  return s.str();
}
//-----------------------------------------------------------------------------
void BlockMatrix::mult(const BlockVector& x, BlockVector& y,
                       bool transposed) const
{
  if (transposed)
    error("BlockMatrix::mult: transposed not implemented.");

  DefaultFactory factory;
  boost::scoped_ptr<GenericVector> vec(factory.create_vector());
  for(uint i = 0; i < matrices.size(); i++)
  {
    y.get(i).resize(this->get(i, 0).size(0));
    vec->resize(y.get(i).size());

    // FIXME: Do we need to zero the vector?
    y.get(i).zero();
    vec->zero();
    for(uint j = 0; j < matrices.size(); j++)
    {
      this->get(i, j).mult(x.get(j), *vec);
      y.get(i) += *vec;
    }
  }
}
//-----------------------------------------------------------------------------
SubMatrix BlockMatrix::operator()(uint i, uint j)
{
  SubMatrix sm(i, j, *this);
  return sm;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
SubMatrix::SubMatrix(uint col, uint row, BlockMatrix& bm)
  : row(row), col(col), bm(bm)
{
  // Do nothing
}
//-----------------------------------------------------------------------------
SubMatrix::~SubMatrix()
{
  // Do nothing
}
//-----------------------------------------------------------------------------
const SubMatrix& SubMatrix::operator=(GenericMatrix& m)
{
  bm.set(row, col, m);
  return *this;
}
//-----------------------------------------------------------------------------
