// ---------------------------------------------------------------------
//
// Copyright (C) 2005 - 2018 by the deal.II authors
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



// like _01 but instead for random points on the face. note that the normal
// vector on a bilinearly mapped face is also a bilinear function



#include <deal.II/base/quadrature_lib.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/manifold.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>

#include "../tests.h"



void
create_triangulation(const unsigned int case_no, Triangulation<2> &tria)
{
  switch (case_no)
    {
      case 0:
        GridGenerator::hyper_cube(tria, 1., 3.);
        break;
      case 1:
        {
          GridGenerator::hyper_cube(tria, 1., 3.);
          Point<2> &v0 = tria.begin_active()->vertex(0);
          v0           = Point<2>(-0.5, -1);
          Point<2> &v1 = tria.begin_active()->vertex(1);
          v1           = Point<2>(1.25, 0.25);
          break;
        }
      default:
        Assert(false, ExcNotImplemented());
    };
}



int
main()
{
  initlog();
  deallog << std::setprecision(3);
  deallog << std::fixed;

  FE_Q<1> linear_interpolator(1);

  Triangulation<2>               tria;
  FlatManifold<2>                boundary;
  Manifold<2>::FaceVertexNormals normals;
  for (unsigned int case_no = 0; case_no < 2; ++case_no)
    {
      deallog << "Case" << case_no << std::endl;
      create_triangulation(case_no, tria);
      const Triangulation<2>::active_cell_iterator cell = tria.begin_active();
      Triangulation<2>::face_iterator              face;
      for (const unsigned int face_no : GeometryInfo<2>::face_indices())
        {
          face = cell->face(face_no);
          boundary.get_normals_at_vertices(face, normals);

          for (double xi = 0; xi <= 1; xi += 0.234)
            {
              Point<2>     p;
              Tensor<1, 2> normal;

              for (unsigned int v = 0; v < GeometryInfo<2>::vertices_per_face;
                   ++v)
                {
                  p += face->vertex(v) *
                       linear_interpolator.shape_value(v, Point<1>(xi));
                  normal += normals[v] *
                            linear_interpolator.shape_value(v, Point<1>(xi));
                }
              normal /= normal.norm();

              deallog << "p=" << p << ", n=" << boundary.normal_vector(face, p)
                      << std::endl;

              Assert((boundary.normal_vector(face, p) - normal).norm() < 1e-10,
                     ExcInternalError());
            }
        }

      tria.clear();
    }

  deallog << "OK" << std::endl;
}
