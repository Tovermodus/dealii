// ---------------------------------------------------------------------
//
// Copyright (C) 2003 - 2018 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------


#include <deal.II/base/qprojector.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/std_cxx14/memory.h>

#include <deal.II/dofs/dof_accessor.h>

#include <deal.II/fe/fe.h>
#include <deal.II/fe/fe_nothing.h>
#include <deal.II/fe/fe_raviart_thomas.h>
#include <deal.II/fe/fe_tools.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/fe/mapping.h>

#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_iterator.h>

#include <sstream>


DEAL_II_NAMESPACE_OPEN


template <int dim>
FE_RaviartThomasNodal<dim>::FE_RaviartThomasNodal(const unsigned int deg)
  : FE_PolyTensor<dim>(PolynomialsRaviartThomas<dim>(deg),
                       FiniteElementData<dim>(get_dpo_vector(deg),
                                              dim,
                                              deg + 1,
                                              FiniteElementData<dim>::Hdiv),
                       get_ria_vector(deg),
                       std::vector<ComponentMask>(
                         PolynomialsRaviartThomas<dim>::n_polynomials(deg),
                         std::vector<bool>(dim, true)))
{
	std::cout << "ravinit\n";
  Assert(dim >= 2, ExcImpossibleInDim(dim));
  const unsigned int n_dofs = this->dofs_per_cell;

	std::cout << "ravinit\n";
  this->mapping_kind = {mapping_raviart_thomas};
  // First, initialize the
  // generalized support points and
  // quadrature weights, since they
  // are required for interpolation.
	std::cout << "ravinit\n";
  initialize_support_points(deg);

  // Now compute the inverse node matrix, generating the correct
  // basis functions from the raw ones. For a discussion of what
  // exactly happens here, see FETools::compute_node_matrix.
  const FullMatrix<double> M = FETools::compute_node_matrix(*this);
  this->inverse_node_matrix.reinit(n_dofs, n_dofs);
  this->inverse_node_matrix.invert(M);
  // From now on, the shape functions provided by FiniteElement::shape_value
  // and similar functions will be the correct ones, not
  // the raw shape functions from the polynomial space anymore.

  // Reinit the vectors of
  // prolongation matrices to the
  // right sizes. There are no
  // restriction matrices implemented
  for (unsigned int ref_case = RefinementCase<dim>::cut_x;
       ref_case < RefinementCase<dim>::isotropic_refinement + 1;
       ++ref_case)
    {
      const unsigned int nc =
        GeometryInfo<dim>::n_children(RefinementCase<dim>(ref_case));

      for (unsigned int i = 0; i < nc; ++i)
        this->prolongation[ref_case - 1][i].reinit(n_dofs, n_dofs);
    }
  // Fill prolongation matrices with embedding operators
  FETools::compute_embedding_matrices(*this, this->prolongation);
  // TODO[TL]: for anisotropic refinement we will probably need a table of
  // submatrices with an array for each refine case
  FullMatrix<double> face_embeddings[GeometryInfo<dim>::max_children_per_face];
  for (unsigned int i = 0; i < GeometryInfo<dim>::max_children_per_face; ++i)
    face_embeddings[i].reinit(this->dofs_per_face, this->dofs_per_face);
  FETools::compute_face_embedding_matrices<dim, double>(*this,
                                                        face_embeddings,
                                                        0,
                                                        0);
  this->interface_constraints.reinit((1 << (dim - 1)) * this->dofs_per_face,
                                     this->dofs_per_face);
  unsigned int target_row = 0;
  for (unsigned int d = 0; d < GeometryInfo<dim>::max_children_per_face; ++d)
    for (unsigned int i = 0; i < face_embeddings[d].m(); ++i)
      {
        for (unsigned int j = 0; j < face_embeddings[d].n(); ++j)
          this->interface_constraints(target_row, j) = face_embeddings[d](i, j);
        ++target_row;
      }
}



template <int dim>
std::string
FE_RaviartThomasNodal<dim>::get_name() const
{
  // note that the
  // FETools::get_fe_by_name
  // function depends on the
  // particular format of the string
  // this function returns, so they
  // have to be kept in synch

  // note that this->degree is the maximal
  // polynomial degree and is thus one higher
  // than the argument given to the
  // constructor
  std::ostringstream namebuf;
  namebuf << "FE_RaviartThomasNodal<" << dim << ">(" << this->degree - 1 << ")";

  return namebuf.str();
}


template <int dim>
std::unique_ptr<FiniteElement<dim, dim>>
FE_RaviartThomasNodal<dim>::clone() const
{
  return std_cxx14::make_unique<FE_RaviartThomasNodal<dim>>(*this);
}


//---------------------------------------------------------------------------
// Auxiliary and internal functions
//---------------------------------------------------------------------------



template <int dim>
void
FE_RaviartThomasNodal<dim>::initialize_support_points(const unsigned int deg)
{
	std::cout << "lll\n";
  this->generalized_support_points.resize(this->dofs_per_cell);
  this->generalized_face_support_points.resize(this->dofs_per_face);

  // Number of the point being entered
  unsigned int current = 0;

  // On the faces, we choose as many
  // Gauss points as necessary to
  // determine the normal component
  // uniquely. This is the deg of
  // the Raviart-Thomas element plus
  // one.
  if (dim > 1)
    {
      QGauss<dim - 1> face_points(deg + 1);
      Assert(face_points.size() == this->dofs_per_face, ExcInternalError());
      for (unsigned int k = 0; k < this->dofs_per_face; ++k)
        this->generalized_face_support_points[k] = face_points.point(k);
      Quadrature<dim> faces =
        QProjector<dim>::project_to_all_faces(face_points);
      for (unsigned int k = 0;
           k < this->dofs_per_face * GeometryInfo<dim>::faces_per_cell;
           ++k)
        this->generalized_support_points[k] =
          faces.point(k + QProjector<dim>::DataSetDescriptor::face(
                            0, true, false, false, this->dofs_per_face));

      current = this->dofs_per_face * GeometryInfo<dim>::faces_per_cell;
    }
  
     int face_points_per_dimension = 2*std::pow(deg+1,dim-1);

	std::cout << "lll\n";
  if (deg == 0)
  {
	  interior_points_per_dimension = 0;
	  points_per_dimension = 2*face_points_per_dimension;
	  compute_tensor_product_basis(deg);
	  sort_generalized_support_points_lexicographically();
	std::cout << "d0\n";
    return;
  }// In the interior, we need
  // anisotropic Gauss quadratures,
  // different for each direction.
  QGauss<1> high(deg + 1);
  QGauss<1> low(deg);

  for (unsigned int d = 0; d < dim; ++d)
    {
      std::unique_ptr<QAnisotropic<dim>> quadrature;
      switch (dim)
        {
          case 1:
            quadrature = std_cxx14::make_unique<QAnisotropic<dim>>(high);
            break;
          case 2:
            quadrature = std_cxx14::make_unique<QAnisotropic<dim>>(
              ((d == 0) ? low : high), ((d == 1) ? low : high));
            break;
          case 3:
            quadrature =
              std_cxx14::make_unique<QAnisotropic<dim>>(((d == 0) ? low : high),
                                                        ((d == 1) ? low : high),
                                                        ((d == 2) ? low :
                                                                    high));
            break;
          default:
            Assert(false, ExcNotImplemented());
        }
      interior_points_per_dimension = quadrature->size();
      for (unsigned int k = 0; k < quadrature->size(); ++k)
        this->generalized_support_points[current++] = quadrature->point(k);
    }
	std::cout << "lll\n";
  points_per_dimension = interior_points_per_dimension + face_points_per_dimension;
  std::cout << "hallooooo\n";
  compute_tensor_product_basis(deg);
  sort_generalized_support_points_lexicographically();
  Assert(current == this->dofs_per_cell, ExcInternalError());
}



template <int dim>
std::vector<unsigned int>
FE_RaviartThomasNodal<dim>::get_dpo_vector(const unsigned int deg)
{
  // the element is face-based and we have
  // (deg+1)^(dim-1) DoFs per face
  unsigned int dofs_per_face = 1;
  for (unsigned int d = 1; d < dim; ++d)
    dofs_per_face *= deg + 1;

  // and then there are interior dofs
  const unsigned int interior_dofs = dim * deg * dofs_per_face;

  std::vector<unsigned int> dpo(dim + 1);
  dpo[dim - 1] = dofs_per_face;
  dpo[dim]     = interior_dofs;

  return dpo;
}



template <>
std::vector<bool>
FE_RaviartThomasNodal<1>::get_ria_vector(const unsigned int)
{
  Assert(false, ExcImpossibleInDim(1));
  return std::vector<bool>();
}



template <int dim>
std::vector<bool>
FE_RaviartThomasNodal<dim>::get_ria_vector(const unsigned int deg)
{
  const unsigned int dofs_per_cell =
    PolynomialsRaviartThomas<dim>::n_polynomials(deg);
  unsigned int dofs_per_face = deg + 1;
  for (unsigned int d = 2; d < dim; ++d)
    dofs_per_face *= deg + 1;
  // all face dofs need to be
  // non-additive, since they have
  // continuity requirements.
  // however, the interior dofs are
  // made additive
  std::vector<bool> ret_val(dofs_per_cell, false);
  for (unsigned int i = GeometryInfo<dim>::faces_per_cell * dofs_per_face;
       i < dofs_per_cell;
       ++i)
    ret_val[i] = true;

  return ret_val;
}


template <int dim>
bool
FE_RaviartThomasNodal<dim>::has_support_on_face(
  const unsigned int shape_index,
  const unsigned int face_index) const
{
  AssertIndexRange(shape_index, this->dofs_per_cell);
  AssertIndexRange(face_index, GeometryInfo<dim>::faces_per_cell);

  // The first degrees of freedom are
  // on the faces and each face has
  // degree degrees.
  const unsigned int support_face = shape_index / this->degree;

  // The only thing we know for sure
  // is that shape functions with
  // support on one face are zero on
  // the opposite face.
  if (support_face < GeometryInfo<dim>::faces_per_cell)
    return (face_index != GeometryInfo<dim>::opposite_face[support_face]);

  // In all other cases, return true,
  // which is safe
  return true;
}



template <int dim>
void
FE_RaviartThomasNodal<dim>::
  convert_generalized_support_point_values_to_dof_values(
    const std::vector<Vector<double>> &support_point_values,
    std::vector<double> &              nodal_values) const
{
  Assert(support_point_values.size() == this->generalized_support_points.size(),
         ExcDimensionMismatch(support_point_values.size(),
                              this->generalized_support_points.size()));
  Assert(nodal_values.size() == this->dofs_per_cell,
         ExcDimensionMismatch(nodal_values.size(), this->dofs_per_cell));
  Assert(support_point_values[0].size() == this->n_components(),
         ExcDimensionMismatch(support_point_values[0].size(),
                              this->n_components()));

  // First do interpolation on
  // faces. There, the component
  // evaluated depends on the face
  // direction and orientation.
  unsigned int fbase = 0;
  unsigned int f     = 0;
  for (; f < GeometryInfo<dim>::faces_per_cell;
       ++f, fbase += this->dofs_per_face)
    {
      for (unsigned int i = 0; i < this->dofs_per_face; ++i)
        {
          nodal_values[fbase + i] = support_point_values[fbase + i](
            GeometryInfo<dim>::unit_normal_direction[f]);
        }
    }

  // The remaining points form dim
  // chunks, one for each component.
  const unsigned int istep = (this->dofs_per_cell - fbase) / dim;
  Assert((this->dofs_per_cell - fbase) % dim == 0, ExcInternalError());

  f = 0;
  while (fbase < this->dofs_per_cell)
    {
      for (unsigned int i = 0; i < istep; ++i)
        {
          nodal_values[fbase + i] = support_point_values[fbase + i](f);
        }
      fbase += istep;
      ++f;
    }
  Assert(fbase == this->dofs_per_cell, ExcInternalError());
}



// TODO: There are tests that check that the following few functions don't
// produce assertion failures, but none that actually check whether they do the
// right thing. one example for such a test would be to project a function onto
// an hp space and make sure that the convergence order is correct with regard
// to the lowest used polynomial degree

template <int dim>
bool
FE_RaviartThomasNodal<dim>::hp_constraints_are_implemented() const
{
  return true;
}


template <int dim>
std::vector<std::pair<unsigned int, unsigned int>>
FE_RaviartThomasNodal<dim>::hp_vertex_dof_identities(
  const FiniteElement<dim> &fe_other) const
{
  // we can presently only compute these
  // identities if both FEs are
  // FE_RaviartThomasNodals or the other is FE_Nothing.
  // In either case, no dofs are assigned on the vertex,
  // so we shouldn't be getting here at all.
  if (dynamic_cast<const FE_RaviartThomasNodal<dim> *>(&fe_other) != nullptr)
    return std::vector<std::pair<unsigned int, unsigned int>>();
  else if (dynamic_cast<const FE_Nothing<dim> *>(&fe_other) != nullptr)
    return std::vector<std::pair<unsigned int, unsigned int>>();
  else
    {
      Assert(false, ExcNotImplemented());
      return std::vector<std::pair<unsigned int, unsigned int>>();
    }
}



template <int dim>
std::vector<std::pair<unsigned int, unsigned int>>
FE_RaviartThomasNodal<dim>::hp_line_dof_identities(
  const FiniteElement<dim> &fe_other) const
{
  // we can presently only compute
  // these identities if both FEs are
  // FE_RaviartThomasNodals or if the other
  // one is FE_Nothing
  if (const FE_RaviartThomasNodal<dim> *fe_q_other =
        dynamic_cast<const FE_RaviartThomasNodal<dim> *>(&fe_other))
    {
      // dofs are located on faces; these are
      // only lines in 2d
      if (dim != 2)
        return std::vector<std::pair<unsigned int, unsigned int>>();

      // dofs are located along lines, so two
      // dofs are identical only if in the
      // following two cases (remember that
      // the face support points are Gauss
      // points):
      // 1. this->degree = fe_q_other->degree,
      //   in the case, all the dofs on
      //   the line are identical
      // 2. this->degree-1 and fe_q_other->degree-1
      //   are both even, i.e. this->dof_per_line
      //   and fe_q_other->dof_per_line are both odd,
      //   there exists only one point (the middle one)
      //   such that dofs are identical on this point
      //
      // to understand this, note that
      // this->degree is the *maximal*
      // polynomial degree, and is thus one
      // higher than the argument given to
      // the constructor
      const unsigned int p = this->degree - 1;
      const unsigned int q = fe_q_other->degree - 1;

      std::vector<std::pair<unsigned int, unsigned int>> identities;

      if (p == q)
        for (unsigned int i = 0; i < p + 1; ++i)
          identities.emplace_back(i, i);

      else if (p % 2 == 0 && q % 2 == 0)
        identities.emplace_back(p / 2, q / 2);

      return identities;
    }
  else if (dynamic_cast<const FE_Nothing<dim> *>(&fe_other) != nullptr)
    {
      // the FE_Nothing has no degrees of freedom, so there are no
      // equivalencies to be recorded
      return std::vector<std::pair<unsigned int, unsigned int>>();
    }
  else
    {
      Assert(false, ExcNotImplemented());
      return std::vector<std::pair<unsigned int, unsigned int>>();
    }
}


template <int dim>
std::vector<std::pair<unsigned int, unsigned int>>
FE_RaviartThomasNodal<dim>::hp_quad_dof_identities(
  const FiniteElement<dim> &fe_other) const
{
  // we can presently only compute
  // these identities if both FEs are
  // FE_RaviartThomasNodals or if the other
  // one is FE_Nothing
  if (const FE_RaviartThomasNodal<dim> *fe_q_other =
        dynamic_cast<const FE_RaviartThomasNodal<dim> *>(&fe_other))
    {
      // dofs are located on faces; these are
      // only quads in 3d
      if (dim != 3)
        return std::vector<std::pair<unsigned int, unsigned int>>();

      // this works exactly like the line
      // case above
      const unsigned int p = this->dofs_per_quad;
      const unsigned int q = fe_q_other->dofs_per_quad;

      std::vector<std::pair<unsigned int, unsigned int>> identities;

      if (p == q)
        for (unsigned int i = 0; i < p; ++i)
          identities.emplace_back(i, i);

      else if (p % 2 != 0 && q % 2 != 0)
        identities.emplace_back(p / 2, q / 2);

      return identities;
    }
  else if (dynamic_cast<const FE_Nothing<dim> *>(&fe_other) != nullptr)
    {
      // the FE_Nothing has no degrees of freedom, so there are no
      // equivalencies to be recorded
      return std::vector<std::pair<unsigned int, unsigned int>>();
    }
  else
    {
      Assert(false, ExcNotImplemented());
      return std::vector<std::pair<unsigned int, unsigned int>>();
    }
}


template <int dim>
FiniteElementDomination::Domination
FE_RaviartThomasNodal<dim>::compare_for_domination(
  const FiniteElement<dim> &fe_other,
  const unsigned int        codim) const
{
  Assert(codim <= dim, ExcImpossibleInDim(dim));
  (void)codim;

  // vertex/line/face/cell domination
  // --------------------------------
  if (const FE_RaviartThomasNodal<dim> *fe_rt_nodal_other =
        dynamic_cast<const FE_RaviartThomasNodal<dim> *>(&fe_other))
    {
      if (this->degree < fe_rt_nodal_other->degree)
        return FiniteElementDomination::this_element_dominates;
      else if (this->degree == fe_rt_nodal_other->degree)
        return FiniteElementDomination::either_element_can_dominate;
      else
        return FiniteElementDomination::other_element_dominates;
    }
  else if (const FE_Nothing<dim> *fe_nothing =
             dynamic_cast<const FE_Nothing<dim> *>(&fe_other))
    {
      if (fe_nothing->is_dominating())
        return FiniteElementDomination::other_element_dominates;
      else
        // the FE_Nothing has no degrees of freedom and it is typically used
        // in a context where we don't require any continuity along the
        // interface
        return FiniteElementDomination::no_requirements;
    }

  Assert(false, ExcNotImplemented());
  return FiniteElementDomination::neither_element_dominates;
}



template <>
void
FE_RaviartThomasNodal<1>::get_face_interpolation_matrix(
  const FiniteElement<1, 1> & /*x_source_fe*/,
  FullMatrix<double> & /*interpolation_matrix*/) const
{
  Assert(false, ExcImpossibleInDim(1));
}


template <>
void
FE_RaviartThomasNodal<1>::get_subface_interpolation_matrix(
  const FiniteElement<1, 1> & /*x_source_fe*/,
  const unsigned int /*subface*/,
  FullMatrix<double> & /*interpolation_matrix*/) const
{
  Assert(false, ExcImpossibleInDim(1));
}



template <int dim>
void
FE_RaviartThomasNodal<dim>::get_face_interpolation_matrix(
  const FiniteElement<dim> &x_source_fe,
  FullMatrix<double> &      interpolation_matrix) const
{
  // this is only implemented, if the
  // source FE is also a
  // RaviartThomasNodal element
  AssertThrow((x_source_fe.get_name().find("FE_RaviartThomasNodal<") == 0) ||
                (dynamic_cast<const FE_RaviartThomasNodal<dim> *>(
                   &x_source_fe) != nullptr),
              typename FiniteElement<dim>::ExcInterpolationNotImplemented());

  Assert(interpolation_matrix.n() == this->dofs_per_face,
         ExcDimensionMismatch(interpolation_matrix.n(), this->dofs_per_face));
  Assert(interpolation_matrix.m() == x_source_fe.dofs_per_face,
         ExcDimensionMismatch(interpolation_matrix.m(),
                              x_source_fe.dofs_per_face));

  // ok, source is a RaviartThomasNodal element, so
  // we will be able to do the work
  const FE_RaviartThomasNodal<dim> &source_fe =
    dynamic_cast<const FE_RaviartThomasNodal<dim> &>(x_source_fe);

  // Make sure, that the element,
  // for which the DoFs should be
  // constrained is the one with
  // the higher polynomial degree.
  // Actually the procedure will work
  // also if this assertion is not
  // satisfied. But the matrices
  // produced in that case might
  // lead to problems in the
  // hp procedures, which use this
  // method.
  Assert(this->dofs_per_face <= source_fe.dofs_per_face,
         typename FiniteElement<dim>::ExcInterpolationNotImplemented());

  // generate a quadrature
  // with the generalized support points.
  // This is later based as a
  // basis for the QProjector,
  // which returns the support
  // points on the face.
  Quadrature<dim - 1> quad_face_support(
    source_fe.generalized_face_support_points);

  // Rule of thumb for FP accuracy,
  // that can be expected for a
  // given polynomial degree.
  // This value is used to cut
  // off values close to zero.
  double eps = 2e-13 * this->degree * (dim - 1);

  // compute the interpolation
  // matrix by simply taking the
  // value at the support points.
  const Quadrature<dim> face_projection =
    QProjector<dim>::project_to_face(quad_face_support, 0);

  for (unsigned int i = 0; i < source_fe.dofs_per_face; ++i)
    {
      const Point<dim> &p = face_projection.point(i);

      for (unsigned int j = 0; j < this->dofs_per_face; ++j)
        {
          double matrix_entry =
            this->shape_value_component(this->face_to_cell_index(j, 0), p, 0);

          // Correct the interpolated
          // value. I.e. if it is close
          // to 1 or 0, make it exactly
          // 1 or 0. Unfortunately, this
          // is required to avoid problems
          // with higher order elements.
          if (std::fabs(matrix_entry - 1.0) < eps)
            matrix_entry = 1.0;
          if (std::fabs(matrix_entry) < eps)
            matrix_entry = 0.0;

          interpolation_matrix(i, j) = matrix_entry;
        }
    }

  // make sure that the row sum of
  // each of the matrices is 1 at
  // this point. this must be so
  // since the shape functions sum up
  // to 1
  for (unsigned int j = 0; j < source_fe.dofs_per_face; ++j)
    {
      double sum = 0.;

      for (unsigned int i = 0; i < this->dofs_per_face; ++i)
        sum += interpolation_matrix(j, i);

      Assert(std::fabs(sum - 1) < 2e-13 * this->degree * (dim - 1),
             ExcInternalError());
    }
}


template <int dim>
void
FE_RaviartThomasNodal<dim>::get_subface_interpolation_matrix(
  const FiniteElement<dim> &x_source_fe,
  const unsigned int        subface,
  FullMatrix<double> &      interpolation_matrix) const
{
  // this is only implemented, if the
  // source FE is also a
  // RaviartThomasNodal element
  AssertThrow((x_source_fe.get_name().find("FE_RaviartThomasNodal<") == 0) ||
                (dynamic_cast<const FE_RaviartThomasNodal<dim> *>(
                   &x_source_fe) != nullptr),
              typename FiniteElement<dim>::ExcInterpolationNotImplemented());

  Assert(interpolation_matrix.n() == this->dofs_per_face,
         ExcDimensionMismatch(interpolation_matrix.n(), this->dofs_per_face));
  Assert(interpolation_matrix.m() == x_source_fe.dofs_per_face,
         ExcDimensionMismatch(interpolation_matrix.m(),
                              x_source_fe.dofs_per_face));

  // ok, source is a RaviartThomasNodal element, so
  // we will be able to do the work
  const FE_RaviartThomasNodal<dim> &source_fe =
    dynamic_cast<const FE_RaviartThomasNodal<dim> &>(x_source_fe);

  // Make sure, that the element,
  // for which the DoFs should be
  // constrained is the one with
  // the higher polynomial degree.
  // Actually the procedure will work
  // also if this assertion is not
  // satisfied. But the matrices
  // produced in that case might
  // lead to problems in the
  // hp procedures, which use this
  // method.
  Assert(this->dofs_per_face <= source_fe.dofs_per_face,
         typename FiniteElement<dim>::ExcInterpolationNotImplemented());

  // generate a quadrature
  // with the generalized support points.
  // This is later based as a
  // basis for the QProjector,
  // which returns the support
  // points on the face.
  Quadrature<dim - 1> quad_face_support(
    source_fe.generalized_face_support_points);

  // Rule of thumb for FP accuracy,
  // that can be expected for a
  // given polynomial degree.
  // This value is used to cut
  // off values close to zero.
  double eps = 2e-13 * this->degree * (dim - 1);

  // compute the interpolation
  // matrix by simply taking the
  // value at the support points.

  const Quadrature<dim> subface_projection =
    QProjector<dim>::project_to_subface(quad_face_support, 0, subface);

  for (unsigned int i = 0; i < source_fe.dofs_per_face; ++i)
    {
      const Point<dim> &p = subface_projection.point(i);

      for (unsigned int j = 0; j < this->dofs_per_face; ++j)
        {
          double matrix_entry =
            this->shape_value_component(this->face_to_cell_index(j, 0), p, 0);

          // Correct the interpolated
          // value. I.e. if it is close
          // to 1 or 0, make it exactly
          // 1 or 0. Unfortunately, this
          // is required to avoid problems
          // with higher order elements.
          if (std::fabs(matrix_entry - 1.0) < eps)
            matrix_entry = 1.0;
          if (std::fabs(matrix_entry) < eps)
            matrix_entry = 0.0;

          interpolation_matrix(i, j) = matrix_entry;
        }
    }

  // make sure that the row sum of
  // each of the matrices is 1 at
  // this point. this must be so
  // since the shape functions sum up
  // to 1
  for (unsigned int j = 0; j < source_fe.dofs_per_face; ++j)
    {
      double sum = 0.;

      for (unsigned int i = 0; i < this->dofs_per_face; ++i)
        sum += interpolation_matrix(j, i);

      Assert(std::fabs(sum - 1) < 2e-13 * this->degree * (dim - 1),
             ExcInternalError());
    }
}



/*
 * Functions for real tensor product behaviour
 */


/*
 * We replicate the behaviour of PolyTensor::shape_value_component by composing
 * the value from one dimensional polynomials. Currently only in two dimensions
 */
template <int dim>
double
FE_RaviartThomasNodal<dim>::shape_value_component_as_tensor_product(
  const unsigned int i,
  const Point<dim> & p,
  const unsigned int component)
{
  unsigned int lexicographic_index = normal_index_to_lexicographic(i);
  unsigned int d                   = lexicographic_index /
                   this->points_per_dimension; // d is the component where the
  // i-th basisfunction has support
  if (d != component)
    return 0.;
  unsigned int index_in_dimension =
    lexicographic_index % this->points_per_dimension;
  if (dim == 1)
    {
      double xpolval = nodal_basis_of_high[index_in_dimension].value(p(0));
      return xpolval;
    }
  if (dim == 2)
    {
      unsigned int xindex = 0;
      unsigned int yindex = 0;
      if (d == 0) // evaluate either one dimensional basisfunction for high
                  // dimensional space i.e. Q_k+1 or low i.e. Q_k
        {
          xindex         = index_in_dimension / this->nodal_basis_of_low.size();
          yindex         = index_in_dimension % this->nodal_basis_of_low.size();
          double xpolval = nodal_basis_of_high[xindex].value(p(0));
          double ypolval = nodal_basis_of_low[yindex].value(p(1));
          return xpolval * ypolval;
        }
      else
        {
          xindex = index_in_dimension / this->nodal_basis_of_high.size();
          yindex = index_in_dimension % this->nodal_basis_of_high.size();
          double xpolval = nodal_basis_of_low[xindex].value(p(0));
          double ypolval = nodal_basis_of_high[yindex].value(p(1));
          return xpolval * ypolval;
        }
    }
  if (dim == 3)
    {
      unsigned int xindex = 0;
      unsigned int yindex = 0;
      unsigned int zindex = 0;
      if (d == 0)
        {
          xindex = index_in_dimension / (this->nodal_basis_of_low.size() *
                                         this->nodal_basis_of_low.size());
          yindex = (index_in_dimension % (this->nodal_basis_of_low.size() *
                                          this->nodal_basis_of_low.size())) /
                   this->nodal_basis_of_low.size();
          zindex = (index_in_dimension % (this->nodal_basis_of_low.size() *
                                          this->nodal_basis_of_low.size())) %
                   this->nodal_basis_of_low.size();
          double xpolval = nodal_basis_of_high[xindex].value(p(0));
          double ypolval = nodal_basis_of_low[yindex].value(p(1));
          double zpolval = nodal_basis_of_low[zindex].value(p(2));
          return xpolval * ypolval * zpolval;
        }
      if (d == 1)
        {
          xindex = index_in_dimension / (this->nodal_basis_of_high.size() *
                                         this->nodal_basis_of_low.size());
          yindex = (index_in_dimension % (this->nodal_basis_of_high.size() *
                                          this->nodal_basis_of_low.size())) /
                   this->nodal_basis_of_low.size();
          zindex = (index_in_dimension % (this->nodal_basis_of_high.size() *
                                          this->nodal_basis_of_low.size())) %
                   this->nodal_basis_of_low.size();
          double xpolval = nodal_basis_of_low[xindex].value(p(0));
          double ypolval = nodal_basis_of_high[yindex].value(p(1));
          double zpolval = nodal_basis_of_low[zindex].value(p(2));
          return xpolval * ypolval * zpolval;
        }
      if (d == 2)
        {
          xindex = index_in_dimension / (this->nodal_basis_of_low.size() *
                                         this->nodal_basis_of_high.size());
          yindex = (index_in_dimension % (this->nodal_basis_of_low.size() *
                                          this->nodal_basis_of_high.size())) /
                   this->nodal_basis_of_high.size();
          zindex = (index_in_dimension % (this->nodal_basis_of_low.size() *
                                          this->nodal_basis_of_high.size())) %
                   this->nodal_basis_of_high.size();
          double xpolval = nodal_basis_of_low[xindex].value(p(0));
          double ypolval = nodal_basis_of_low[yindex].value(p(1));
          double zpolval = nodal_basis_of_high[zindex].value(p(2));
          return xpolval * ypolval * zpolval;
        }
    }
  return 0;
}

/*
 * Find out how to translate lexicographic numbering from and to dealii
 * numbering of dofs
 */
template <int dim>
void
FE_RaviartThomasNodal<dim>::sort_generalized_support_points_lexicographically()
{
  assert(this->lexicographic_support_points.size() >
         0); // tensor product bases are not yet generated
  AssertDimension(this->generalized_support_points.size(),
                  lexicographic_support_points.size());
  this->lexicographic_transformation =
    std::vector<unsigned int>(lexicographic_support_points.size());
  this->inverse_lexicographic_transformation =
    std::vector<unsigned int>(lexicographic_support_points.size());
  for (unsigned int d = 0; d < dim; d++)
    {
      /*
       * for each component find faces which have dofs for that component and
       * find the point where dofs for the interior of that component start,
       * then compare support points with the lexicographic support points and
       * save results
       */
      unsigned int face1_begin = 0; // default value which is impossible
      unsigned int face2_begin = 0;
      unsigned int interior_begin =
        GeometryInfo<dim>::faces_per_cell * this->dofs_per_face +
        interior_points_per_dimension * d;
      unsigned int support_point_iterator = 0;
      if (dim > 1)
        {
          for (unsigned int f = 0; f < GeometryInfo<dim>::faces_per_cell; f++)
            {
              if (GeometryInfo<dim>::unit_normal_direction[f] == d)
                {
                  if (face1_begin == 0)
                    face1_begin = support_point_iterator;
                  else
                    face2_begin = support_point_iterator;
                }
              support_point_iterator += this->dofs_per_face;
            }
        }
      for (unsigned int lex_point_index = points_per_dimension * d;
           lex_point_index < points_per_dimension * (d + 1);
           lex_point_index++)
        {
          Point<dim> lexicographic_point =
            lexicographic_support_points[lex_point_index];
          if (dim > 1)
            for (unsigned int i = 0; i < this->dofs_per_face; i++)
              {
                if (this->generalized_support_points[face1_begin + i] ==
                    lexicographic_point)
                  {
                    lexicographic_transformation[face1_begin + i] =
                      lex_point_index;
                    inverse_lexicographic_transformation[lex_point_index] =
                      face1_begin + i;
                  }
                if (this->generalized_support_points[face2_begin + i] ==
                    lexicographic_point)
                  {
                    lexicographic_transformation[face2_begin + i] =
                      lex_point_index;
                    inverse_lexicographic_transformation[lex_point_index] =
                      face2_begin + i;
                  }
              }
          for (unsigned int i = 0; i < this->interior_points_per_dimension; i++)
            {
              if (this->generalized_support_points[interior_begin + i] ==
                  lexicographic_point)
                {
                  lexicographic_transformation[interior_begin + i] =
                    lex_point_index;
                  inverse_lexicographic_transformation[lex_point_index] =
                    interior_begin + i;
                }
            }
        }
    }
}

template <int dim>
unsigned int
FE_RaviartThomasNodal<dim>::lexicographic_index_to_normal(unsigned int i)
{
  return inverse_lexicographic_transformation[i];
}
template <int dim>
unsigned int
FE_RaviartThomasNodal<dim>::normal_index_to_lexicographic(unsigned int i)
{
  return lexicographic_transformation[i];
}
/*
 * Compute one dimensional nodal bases for the "high" dimensional Q_k+1 and
 * "low" dimensional Q_k space.
 */
template <int dim>
void
FE_RaviartThomasNodal<dim>::compute_tensor_product_basis(const unsigned int deg)
{
  assert(this->points_per_dimension != 0); // No degrees of Freedom are defined
  // first define Gauss points for interior, here high and low is switched since
  // low does not have boundary points yet.
  QGauss<1>             high_interior(deg + 1);
  QGauss<1>             low_interior(deg);
  std::vector<Point<1>> low  = high_interior.get_points();
  std::vector<Point<1>> high = {Point<1>(0.)};
  high.insert(high.end(),
              low_interior.get_points().begin(),
              low_interior.get_points().end());
  high.emplace_back(1.); // add boundary points
  std::vector<Polynomials::Polynomial<double>> basis_of_high =
    Polynomials::LagrangeEquidistant::generate_complete_basis(deg + 1);
  std::vector<Polynomials::Polynomial<double>> basis_of_low =
    Polynomials::LagrangeEquidistant::generate_complete_basis(deg);
  // compute node_matrices
  FullMatrix<double> N_high(deg + 2, deg + 2); // node_matrix for Q_k+1
  FullMatrix<double> N_low(deg + 1, deg + 1);  // node matrix for Q_k
  for (std::size_t i = 0; i < deg + 2; i++)
    {
      for (std::size_t j = 0; j < deg + 2; j++)
        {
          if (i < deg + 1 && j < deg + 1)
            {
              N_low(i, j) = basis_of_low[i].value(low[j](0));
            }
          N_high(i, j) = basis_of_high[i].value(high[j](0));
        }
    }

  FullMatrix<double> N_high_invert(deg + 2, deg + 2);
  N_high_invert.invert(N_high);
  FullMatrix<double> N_low_invert(deg + 1, deg + 1);
  N_low_invert.invert(N_low);

  nodal_basis_of_high = std::vector<Polynomials::Polynomial<double>>(deg + 2);
  nodal_basis_of_low  = std::vector<Polynomials::Polynomial<double>>(deg + 1);
  for (std::size_t i = 0; i < deg + 2;
       i++) // apply inverse node matrices to obtain nodal basis
    {
      if (i < deg + 1)
        nodal_basis_of_low[i] = Polynomials::Polynomial<double>(deg + 1);
      nodal_basis_of_high[i] = Polynomials::Polynomial<double>(deg + 2);
      for (std::size_t j = 0; j < deg + 2; j++)
        {
          if (j < deg + 1 && i < deg + 1)
            {
              Polynomials::Polynomial<double> pol  = basis_of_low[j];
              double                          scal = N_low_invert(i, j);
              pol *= scal;
              nodal_basis_of_low[i] += pol;
              std::cout << nodal_basis_of_low[i].value(low[i](0)) << "low\n";
            }
          Polynomials::Polynomial<double> pol  = basis_of_high[j];
          double                          scal = N_high_invert(i, j);
          pol *= scal;
          nodal_basis_of_high[i] += pol;
          std::cout << nodal_basis_of_high[i].value(high[i](0)) << "high\n";
        }
    }
  // construct and number lexicographic support points, such that we can later
  // find the renumbering from normal numbering to lexicographic numbering
  std::cout << "dimension" << dim << "\n";
  switch (dim)
    {
      case 1:
        for (auto &i : high)
          lexicographic_support_points.emplace_back(Point<dim>(i(0)));
        break;
      case 2:
        for (auto &i : high)
          for (auto &j : low)
            lexicographic_support_points.emplace_back(Point<dim>(i(0), j(0)));
        for (auto &i : low)
          for (auto &j : high)
            lexicographic_support_points.emplace_back(Point<dim>(i(0), j(0)));
        break;
      case 3:
        for (auto &i : high)
          for (auto &j : low)
            for (auto &k : low)
              lexicographic_support_points.emplace_back(
                Point<dim>(i(0), j(0), k(0)));
        for (auto &i : low)
          for (auto &j : high)
            for (auto &k : low)
              lexicographic_support_points.emplace_back(
                Point<dim>(i(0), j(0), k(0)));
        for (auto &i : low)
          for (auto &j : low)
            for (auto &k : high)
              lexicographic_support_points.emplace_back(
                Point<dim>(i(0), j(0), k(0)));
        break;
    }
  std::cout << "lexsize" << lexicographic_support_points.size()<<"\n";
}

using namespace internal::MatrixFreeFunctions;
template <int dim>
void
FE_RaviartThomasNodal<dim>::fill_shapeInfo(
  internal::MatrixFreeFunctions::ShapeInfo<double> &shapeInfo,
  Quadrature<1>                                     quad) const
{
	int iiiii = 0;
	std::cout << iiiii++<<"nnn\n";
  UnivariateShapeData<double> lower;
  UnivariateShapeData<double> higher;
  lower.element_type              = tensor_symmetric;
  higher.element_type             = tensor_symmetric;
  std::cout << this->degree<<"FEGRAD\n";
  lower.fe_degree                 = this->degree-1;
  higher.fe_degree                = this->degree;
  lower.quadrature                = quad;
  higher.quadrature               = quad; 
  lower.n_q_points_1d             = quad.size();
  higher.n_q_points_1d            = quad.size();
  lower.nodal_at_cell_boundaries  = false;
  higher.nodal_at_cell_boundaries = true;
 
	std::cout << iiiii++<<"nnn\n";
	lower.shape_values =
    AlignedVector<double>((lower.fe_degree + 1) * lower.n_q_points_1d); //+1
  higher.shape_values =
    AlignedVector<double>((higher.fe_degree + 1) * higher.n_q_points_1d);
  lower.shape_gradients =
    AlignedVector<double>((lower.fe_degree + 1) * lower.n_q_points_1d);
  higher.shape_gradients =
    AlignedVector<double>((higher.fe_degree + 1) * higher.n_q_points_1d);
  lower.shape_hessians =
    AlignedVector<double>((lower.fe_degree + 1) * lower.n_q_points_1d);
  higher.shape_hessians =
    AlignedVector<double>((higher.fe_degree + 1) * higher.n_q_points_1d);
	std::cout << iiiii++<<"nnn\n";
  lower.shape_data_on_face[0].resize(3 * (lower.fe_degree + 1));
  lower.shape_data_on_face[1].resize(3 * (lower.fe_degree + 1));
  higher.shape_data_on_face[0].resize(3 * (higher.fe_degree + 1));
  higher.shape_data_on_face[1].resize(3 * (higher.fe_degree + 1));
	std::cout << iiiii++<<"nnn\n";
  for (std::size_t i = 0; i < (lower.fe_degree + 1); i++)
    {
      for (std::size_t j = 0; j < lower.n_q_points_1d; j++)
        {
          lower.shape_values[i * lower.n_q_points_1d + j] =
            nodal_basis_of_low[i].value(lower.quadrature.point(j)[0]);
          lower.shape_gradients[i * lower.n_q_points_1d + j] =
            nodal_basis_of_low[i].derivative().value(lower.quadrature.point(j)[0]);
          lower.shape_hessians[i * lower.n_q_points_1d + j] =
            nodal_basis_of_low[i].derivative().derivative().value(
              lower.quadrature.point(j)[0]);
        }
      lower.shape_data_on_face[0][i] = nodal_basis_of_low[i].value(0);
      lower.shape_data_on_face[0][i + (lower.fe_degree + 1)] =
        nodal_basis_of_low[i].derivative().value(0);
      lower.shape_data_on_face[0][i + 2 * (lower.fe_degree + 1)] =
        nodal_basis_of_low[i].derivative().derivative().value(0);
      lower.shape_data_on_face[1][i] = nodal_basis_of_low[i].value(j);
      lower.shape_data_on_face[1][i + (lower.fe_degree + 1)] =
        nodal_basis_of_low[i].derivative().value(j);
      lower.shape_data_on_face[1][i + 2 * (lower.fe_degree + 1)] =
        nodal_basis_of_low[i].derivative().derivative().value(j);
    }
	std::cout << iiiii++<<"nnn\n";
  for (std::size_t i = 0; i < (higher.fe_degree + 1); i++)
    {
      for (std::size_t j = 0; j < higher.n_q_points_1d; j++)
        {
          higher.shape_values[i * higher.n_q_points_1d + j] =
            nodal_basis_of_high[i].value(higher.quadrature.point(j)[0]);
          higher.shape_gradients[i * higher.n_q_points_1d + j] =
            nodal_basis_of_high[i].derivative().value(higher.quadrature.point(j)[0]);
          higher.shape_hessians[i * higher.n_q_points_1d + j] =
            nodal_basis_of_high[i].derivative().derivative().value(
              higher.quadrature.point(j)[0]);
        }
      higher.shape_data_on_face[0][i] = nodal_basis_of_high[i].value(0);
      higher.shape_data_on_face[0][i + (higher.fe_degree + 1)] =
        nodal_basis_of_high[i].derivative().value(0);
      higher.shape_data_on_face[0][i + 2 * (higher.fe_degree + 1)] =
        nodal_basis_of_high[i].derivative().derivative().value(0);
      higher.shape_data_on_face[1][i] = nodal_basis_of_high[i].value(j);
      higher.shape_data_on_face[1][i + (higher.fe_degree + 1)] =
        nodal_basis_of_high[i].derivative().value(j);
      higher.shape_data_on_face[1][i + 2 * (higher.fe_degree + 1)] =
        nodal_basis_of_high[i].derivative().derivative().value(j);
    }
	std::cout << iiiii++<<"nnn\n";
  shapeInfo.data.emplace_back(lower);
  shapeInfo.data.emplace_back(higher);
  shapeInfo.lexicographic_numbering = lexicographic_transformation;
  shapeInfo.n_dimensions            = dim;
  shapeInfo.n_components            = dim;
  shapeInfo.element_type = internal::MatrixFreeFunctions::ElementType::raviart_thomas;
  shapeInfo.dofs_per_component_on_cell =
    (higher.fe_degree + 1) * std::pow(lower.fe_degree, dim - 1);
	std::cout << iiiii++<<"nnn\n";
  // dofs_per_component_on_face cannot be set since different faces have
  // different numbers, Maximum number is std::pow(lower.fe_degree,dim-1) other
  // faces have 0
  shapeInfo.dofs_per_component_on_face = -1;
  shapeInfo.n_q_points                 = std::pow(quad.size(), dim);
  shapeInfo.n_q_points_face         = std::pow(quad.size(), dim - 1);
  shapeInfo.data_access.reinit(dim, dim);
	std::cout << iiiii++<<"888nnn\n";
  for (std::size_t i = 0; i < dim; i++)
    {
      for (std::size_t j = 0; j < dim; j++)
        {
		if(i == j)
			shapeInfo.data_access(i,j) = &higher;
		else
			shapeInfo.data_access(i,j) = &lower;
        }
    }
  std::cout << shapeInfo.lexicographic_numbering.size()<<"nnn\n";
}



// explicit instantiations
#include "fe_raviart_thomas_nodal.inst"


DEAL_II_NAMESPACE_CLOSE
