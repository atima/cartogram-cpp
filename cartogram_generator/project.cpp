#include <boost/multi_array.hpp>
#include <iostream>
#include <vector>
#include "interpolate_bilinearly.h"
#include "matrix.h"
#include "densification_points.h"
#include "project.h"

void project(InsetState *inset_state)
{
  const unsigned int lx = inset_state->lx();
  const unsigned int ly = inset_state->ly();
  boost::multi_array<XYPoint, 2> &proj = *inset_state->proj();

  // Calculate displacement from proj array
  boost::multi_array<double, 2> xdisp(boost::extents[lx][ly]);
  boost::multi_array<double, 2> ydisp(boost::extents[lx][ly]);
  for (unsigned int i = 0; i < lx; i++) {
    for (unsigned int j=0; j<ly; j++) {
      xdisp[i][j] = proj[i][j].x - i - 0.5;
      ydisp[i][j] = proj[i][j].y - j - 0.5;
    }
  }
  std::vector<GeoDiv> new_geo_divs;
  for (auto gd : inset_state->geo_divs()) {

    // For each GeoDiv
    GeoDiv new_gd(gd.id());

    for (auto pwh : gd.polygons_with_holes()) {
      // For each polygon with holes

      Polygon old_ext_ring = pwh.outer_boundary();
      Polygon new_ext_ring;

      for (unsigned int i = 0; i < old_ext_ring.size(); i++) {

        // Update exterior ring coordinates
        double old_ext_intp_x =
          interpolate_bilinearly(old_ext_ring[i][0], old_ext_ring[i][1],
                                 &xdisp, 'x',
                                 lx, ly);
        double old_ext_intp_y =
          interpolate_bilinearly(old_ext_ring[i][0], old_ext_ring[i][1],
                                 &ydisp, 'y',
                                 lx, ly);
        new_ext_ring.push_back(Point(old_ext_intp_x +old_ext_ring[i][0],
                                     old_ext_intp_y + old_ext_ring[i][1]));
      }
      std::vector<Polygon> hole_v;
      for (auto hci = pwh.holes_begin(); hci != pwh.holes_end(); hci++) {
        Polygon old_hole = *hci;
        Polygon new_hole;
        for (unsigned int i = 0; i < old_hole.size(); i++) {

          // Update hole coordinates
          double old_hole_intp_x =
            interpolate_bilinearly(old_hole[i][0], old_hole[i][1],
                                   &xdisp, 'x',
                                   lx, ly);
          double old_hole_intp_y =
            interpolate_bilinearly(old_hole[i][0], old_hole[i][1],
                                   &ydisp, 'y', lx, ly);
            new_hole.push_back(Point(old_hole_intp_x + old_hole[i][0],
                                     old_hole_intp_y + old_hole[i][1]));
        }
        hole_v.push_back(new_hole);
      }
      const Polygon_with_holes new_pwh(new_ext_ring,
                                       hole_v.begin(),
                                       hole_v.end());
      new_gd.push_back(new_pwh);
    }
    new_geo_divs.push_back(new_gd);
  }
  inset_state->set_geo_divs(new_geo_divs);
  return;
}

void choose_diag(InsetState *inset_state)
{
  const unsigned int lx = inset_state->lx();
  const unsigned int ly = inset_state->ly();

  boost::multi_array<XYPoint, 2> &proj = *inset_state->proj();
  boost::multi_array<int, 2> &graticule_diagonals = *inset_state->graticule_diagonals();

  // Initialize array if running for the first time
  if (inset_state->n_finished_integrations() == 0) {
    graticule_diagonals.resize(boost::extents[lx][ly]);
  }

  int num_concave = 0;

  for (unsigned int i = 0; i < lx - 1; i++){
    for (unsigned int j = 0; j < ly - 1; j++){

      // Get the transformed graticule cell as a Polygon
      Polygon trans_graticule;

      double v0x = proj[i][j].x;
      double v0y = proj[i][j].y;

      double v1x = proj[i + 1][j].x;
      double v1y = proj[i + 1][j].y;

      double v2x = proj[i + 1][j + 1].x;
      double v2y = proj[i + 1][j + 1].y;

      double v3x = proj[i][j + 1].x;
      double v3y = proj[i][j + 1].y;

      trans_graticule.push_back(Point(v0x, v0y));
      trans_graticule.push_back(Point(v1x, v1y));
      trans_graticule.push_back(Point(v2x, v2y));
      trans_graticule.push_back(Point(v3x, v3y));

      // Get the two possible midpoints
      XYPoint midpoint0;
      midpoint0.x = (v0x + v2x) / 2;
      midpoint0.y = (v0y + v2y) / 2;

      XYPoint midpoint1;
      midpoint1.x = (v1x + v3x) / 2;
      midpoint1.y = (v1y + v3y) / 2;

      // Check which midpoint is inside the graticule cell. A complex polygon returns an error.
      if (trans_graticule.bounded_side(Point(midpoint0.x, midpoint0.y)) == CGAL::ON_BOUNDED_SIDE){
        graticule_diagonals[i][j] = 0;
      } else if (trans_graticule.bounded_side(Point(midpoint1.x, midpoint1.y)) == CGAL::ON_BOUNDED_SIDE){
        graticule_diagonals[i][j] = 1;
      } else {
        std::cout << "Invalid graticule cell! At\n";
        std::cout << "(" << v0x << ", " << v0y << ")\n";
        std::cout << "(" << v1x << ", " << v1y << ")\n";
        std::cout << "(" << v2x << ", " << v2y << ")\n";
        std::cout << "(" << v3x << ", " << v3y << ")\n";
        std::cout << "i: " << i << "j: " << j << "\n";
        exit(1);
      }

      if (trans_graticule.is_convex() == false){
        num_concave += 1;
        // std::cout << "Concave graticule cell " << i << " by " << j << "\n";
        // std::cout << "V0: " << v0x << " " << v0y << "\n";
        // std::cout << "V1: " << v1x << " " << v1y << "\n";
        // std::cout << "V2: " << v2x << " " << v2y << "\n";
        // std::cout << "V3: " << v3x << " " << v3y << "\n";
        // std::cout << "Diagonal chosen: " << graticule_diagonals[i][j] << "\n";
      }
    }
  }

  std::cout << "Number of concave graticule cells: " << num_concave << "\n";
  return;
}

std::vector<XYPoint> find_triangle(const double x,
                                   const double y,
                                   const int lx,
                                   const int ly,
                                   boost::multi_array<int, 2> *graticule_diagonals)
{

  if (x < 0 || x > lx || y < 0 || y > ly) {
    std::cerr << "ERROR: coordinate outside bounding box in "
              << "find_triangle().\n";
    std::cerr << "x=" << x << ", y=" << y << std::endl;
    exit(1);
  }

  // Get original graticule coordinates.
  double v0x = floor(x + 0.5) - 0.5;
  double v0y = floor(y + 0.5) - 0.5;

  double v1x = v0x + 1.0;
  double v1y = v0y;

  double v2x = v0x + 1.0;
  double v2y = v0y + 1.0;

  double v3x = v0x;
  double v3y = v0y + 1.0;

  // There are always two triangles in a graticule cell. The coordinates
  // of these triangles differ based on the chosen diagonal.
  Polygon triangle1;
  Polygon triangle2;

  if ((*graticule_diagonals)[int(v0x)][int(v0y)] == 0) {

    triangle1.push_back(Point(v0x, v0y));
    triangle1.push_back(Point(v1x, v1y));
    triangle1.push_back(Point(v2x, v2y));

    triangle2.push_back(Point(v0x, v0y));
    triangle2.push_back(Point(v2x, v2y));
    triangle2.push_back(Point(v3x, v3y));

  } else {

    triangle1.push_back(Point(v0x, v0y));
    triangle1.push_back(Point(v1x, v1y));
    triangle1.push_back(Point(v3x, v3y));

    triangle2.push_back(Point(v1x, v1y));
    triangle2.push_back(Point(v2x, v2y));
    triangle2.push_back(Point(v3x, v3y));

  }

  // Determine which untransformed triangle the given point is in.
  // If the point is in neither, an error is raised.
  std::vector<XYPoint> triangle_coordinates;
  if ((triangle1.bounded_side(Point(x, y)) == CGAL::ON_BOUNDED_SIDE) ||
      (triangle1.bounded_side(Point(x, y)) == CGAL::ON_BOUNDARY)){

    for (unsigned int i = 0; i < triangle1.size(); i++){

      XYPoint triangle_point;
      triangle_point.x = triangle1[i][0];
      triangle_point.y = triangle1[i][1];

      triangle_coordinates.push_back(triangle_point);

    }

  } else if ((triangle2.bounded_side(Point(x, y)) == CGAL::ON_BOUNDED_SIDE) ||
             (triangle2.bounded_side(Point(x, y)) == CGAL::ON_BOUNDARY)) {

    for (unsigned int i = 0; i < triangle2.size(); i++){

      XYPoint triangle_point;
      triangle_point.x = triangle2[i][0];
      triangle_point.y = triangle2[i][1];

      triangle_coordinates.push_back(triangle_point);

    }

  } else {
    std::cout << "Point not in graticule cell!\n";
    exit(1);
  }

  return triangle_coordinates;
}

XYPoint affine_trans(std::vector<XYPoint> *tri,
                     std::vector<XYPoint> *org_tri,
                     double x, double y){

  /*
  For each point, we make the following transformation.
  Suppose we find that, before the cartogram transformation, a point (x, y)
  is in the triangle (a, b, c). We want to find its position in
  the projected triangle (p, q, r). We locally approximate the cartogram
  transformation by an affine transformation T such that T(a) = p,
  T(b) = q and T(c) = r. We can think of T as a 3x3 matrix
   /t11 t12 t13\ 
  | t21 t22 t23 |  such that
   \ 0   0   1 /
   /t11 t12 t13\   /a1 b1 c1\     /p1 q1 r1\ 
  | t21 t22 t23 | | a2 b2 c2 | = | p2 q2 r2 | or TA = P. Hence T = PA^{-1}
   \ 0   0   1 /   \ 1  1  1/     \ 1  1  1/
                               /b2-c2 c1-b1 b1*c2-b2*c1\ 
  We have A^{-1} = (1/det(A)) | c2-a2 a1-c1 a2*c1-a1*c2 |. By multiplying
                               \a2-b2 b1-a1 a1*b2-a2*b1/
  PA^{-1} we obtain t11, t12, t13, t21, t22, t23. The postimage of (x, y) i
  the unprojected map is then "pre" with coordinates
  post.x = t11*x + t12*y + t13, pre.y = t21*x + t22*y + t23.
  */

  XYPoint pre;
  pre.x = x;
  pre.y = y;

  // Old triangle (a, b, c) as a matrix, explained earlier as Matrix A
  Matrix abc_mA((*org_tri)[0], (*org_tri)[1], (*org_tri)[2]);

  // New triangle (p, q, r) as a matrix, explained earlier as Matrix P
  Matrix pqr_mP((*tri)[0], (*tri)[1], (*tri)[2]);

  // Calculating transformation matrix
  Matrix mT = pqr_mP.multiply(abc_mA.inverse());

  // Transforming point and pushing back to temporary_ext_boundary
  XYPoint post = mT.transform_XYPoint(pre);

  return post;
}

void round_points (InsetState *inset_state)
{
  std::vector<GeoDiv> new_geo_divs;

  for (GeoDiv gd : inset_state->geo_divs()) {

    // For each GeoDiv
    GeoDiv new_gd(gd.id());

    for (auto pwh : gd.polygons_with_holes()) {

      // For each polygon with holes
      Polygon old_ext_ring = pwh.outer_boundary();
      Polygon new_ext_ring;

      for (unsigned int i = 0; i < old_ext_ring.size(); i++) {
        new_ext_ring.push_back(round_point(old_ext_ring[i]));
      }

      std::vector<Polygon> hole_v;
      for (auto hci = pwh.holes_begin(); hci != pwh.holes_end(); hci++) {
        Polygon old_hole = *hci;
        Polygon new_hole;
        for (unsigned int i = 0; i < old_hole.size(); i++) {
          new_hole.push_back(round_point(old_hole[i]));
        }
        hole_v.push_back(new_hole);
      }
      const Polygon_with_holes new_pwh(new_ext_ring,
                                       hole_v.begin(),
                                       hole_v.end());
      new_gd.push_back(new_pwh);
    }
    new_geo_divs.push_back(new_gd);
  }
  inset_state->set_geo_divs(new_geo_divs);

  return;
}

void project_with_triangulation(InsetState *inset_state)
{
  const unsigned int lx = inset_state->lx();
  const unsigned int ly = inset_state->ly();
  boost::multi_array<XYPoint, 2> &proj = *inset_state->proj();
  boost::multi_array<int, 2> &graticule_diagonals = *inset_state->graticule_diagonals();

  std::vector<GeoDiv> new_geo_divs;

  for (auto gd : inset_state->geo_divs()) {

    // For each GeoDiv
    GeoDiv new_gd(gd.id());

    for (auto pwh : gd.polygons_with_holes()) {
      // For each polygon with holes

      Polygon old_ext_ring = pwh.outer_boundary();
      Polygon new_ext_ring;

      for (unsigned int i = 0; i < old_ext_ring.size(); i++) {

        // Get the untransformed triangle the point is in.
        std::vector<XYPoint> ext_ring_triangle =
          find_triangle(old_ext_ring[i][0], old_ext_ring[i][1],
                        lx, ly, &graticule_diagonals);

        // Get the coordinates of the transformed triangle.
        std::vector<XYPoint> transformed_ext_ring_triangle;

        XYPoint v0;
        v0.x =
          proj[int(ext_ring_triangle[0].x)][int(ext_ring_triangle[0].y)].x;
        v0.y =
          proj[int(ext_ring_triangle[0].x)][int(ext_ring_triangle[0].y)].y;

        XYPoint v1;
        v1.x =
          proj[int(ext_ring_triangle[1].x)][int(ext_ring_triangle[1].y)].x;
        v1.y =
          proj[int(ext_ring_triangle[1].x)][int(ext_ring_triangle[1].y)].y;

        XYPoint v2;
        v2.x =
          proj[int(ext_ring_triangle[2].x)][int(ext_ring_triangle[2].y)].x;
        v2.y =
          proj[int(ext_ring_triangle[2].x)][int(ext_ring_triangle[2].y)].y;

        transformed_ext_ring_triangle.push_back(v0);
        transformed_ext_ring_triangle.push_back(v1);
        transformed_ext_ring_triangle.push_back(v2);

        // Compute the projected point from the untransformed and transformed
        // triangles
        XYPoint old_ext_ring_intp = affine_trans(&transformed_ext_ring_triangle,
                                             &ext_ring_triangle,
                                             old_ext_ring[i][0], old_ext_ring[i][1]);

        // Update exterior ring coordinates
        new_ext_ring.push_back(round_point(Point(old_ext_ring_intp.x,
                                                 old_ext_ring_intp.y)));
      }
      std::vector<Polygon> hole_v;
      for (auto hci = pwh.holes_begin(); hci != pwh.holes_end(); hci++) {
        Polygon old_hole = *hci;
        Polygon new_hole;
        for (unsigned int i = 0; i < old_hole.size(); i++) {

          std::vector<XYPoint> hole_triangle =
            find_triangle(old_hole[i][0], old_hole[i][1],
                          lx, ly, &graticule_diagonals);

          std::vector<XYPoint> transformed_hole_triangle;

          XYPoint v0;
          v0.x =
            proj[int(hole_triangle[0].x)][int(hole_triangle[0].y)].x;
          v0.y =
            proj[int(hole_triangle[0].x)][int(hole_triangle[0].y)].y;

          XYPoint v1;
          v1.x =
            proj[int(hole_triangle[1].x)][int(hole_triangle[1].y)].x;
          v1.y =
            proj[int(hole_triangle[1].x)][int(hole_triangle[1].y)].y;

          XYPoint v2;
          v2.x =
            proj[int(hole_triangle[2].x)][int(hole_triangle[2].y)].x;
          v2.y =
            proj[int(hole_triangle[2].x)][int(hole_triangle[2].y)].y;

          transformed_hole_triangle.push_back(v0);
          transformed_hole_triangle.push_back(v1);
          transformed_hole_triangle.push_back(v2);

          XYPoint old_hole_intp = affine_trans(&transformed_hole_triangle,
                                               &hole_triangle,
                                               old_hole[i][0], old_hole[i][1]);

          new_hole.push_back(round_point(Point(old_hole_intp.x,
                                               old_hole_intp.y)));
        }
        hole_v.push_back(new_hole);
      }
      const Polygon_with_holes new_pwh(new_ext_ring,
                                       hole_v.begin(),
                                       hole_v.end());
      new_gd.push_back(new_pwh);
    }
    new_geo_divs.push_back(new_gd);
  }
  inset_state->set_geo_divs(new_geo_divs);
  return;
}

// Function for debugging. Returns the original graticule cell
// containing the point of coordinates (x, y)
std::vector<XYPoint> find_graticule(const double x,
                                    const double y)
{
  std::vector<XYPoint> graticule_vertices;

  double v0x = floor(x + 0.5) - 0.5;
  double v0y = floor(y + 0.5) - 0.5;

  double v1x = v0x + 1.0;
  double v1y = v0y;

  double v2x = v0x + 1.0;
  double v2y = v0y + 1.0;

  double v3x = v0x;
  double v3y = v0y + 1.0;

  XYPoint v0;
  v0.x = v0x;
  v0.y = v0y;

  XYPoint v1;
  v1.x = v1x;
  v1.y = v1y;

  XYPoint v2;
  v2.x = v2x;
  v2.y = v2y;

  XYPoint v3;
  v3.x = v3x;
  v3.y = v3y;

  graticule_vertices.push_back(v0);
  graticule_vertices.push_back(v1);
  graticule_vertices.push_back(v2);
  graticule_vertices.push_back(v3);

  return graticule_vertices;
}

// Function for debugging. Search for a point within the given x and y range,
// as well as the points immediately preceding and succeeding this point.
void point_search(InsetState *inset_state,
                  double x_min, double x_max,
                  double y_min, double y_max)
{
  boost::multi_array<int, 2> &graticule_diagonals = *inset_state->graticule_diagonals();

  std::cout << std::setprecision(20);

  for (auto gd : inset_state->geo_divs()) {

    // For each GeoDiv

    for (auto pwh : gd.polygons_with_holes()) {
      // For each polygon with holes

      Polygon old_ext_ring = pwh.outer_boundary();

      for (unsigned int i = 0; i < old_ext_ring.size(); i++) {

        if ((x_min <= old_ext_ring[i][0] && x_max >= old_ext_ring[i][0]) &&
              (y_min <= old_ext_ring[i][1] && y_max >= old_ext_ring[i][1])){

          for (unsigned int a = (i - 1); a < (i + 2); a++){

            std::vector<XYPoint> ext_ring_graticule_cell =
                find_graticule(old_ext_ring[a][0], old_ext_ring[a][1]);

            std::cout << "Point at (" << old_ext_ring[a][0] << ", " << old_ext_ring[a][1] << ")\n";

            std::cout << "On external boundary of " << gd.id() << "\n";

            std::cout << "Point " << a << " out of " << old_ext_ring.size() << "\n";

            std::cout << "Graticule cell cordinates: \n";
            for (unsigned int z = 0; z < 4; z++){
              std::cout << "V" << z << ": (" << ext_ring_graticule_cell[z].x << ", " << ext_ring_graticule_cell[z].y << ")\n";
            }

            std::cout << "Diagonal chosen: " << graticule_diagonals[int(ext_ring_graticule_cell[0].x)][int(ext_ring_graticule_cell[0].y)] << "\n";
          }
        }

      }
      std::vector<Polygon> hole_v;
      for (auto hci = pwh.holes_begin(); hci != pwh.holes_end(); hci++) {

        Polygon old_hole = *hci;

        for (unsigned int i = 0; i < old_hole.size(); i++) {

          if ((x_min <= old_hole[i][0] && x_max >= old_hole[i][0]) &&
              (y_min <= old_hole[i][1] && y_max >= old_hole[i][1])){

            for (unsigned int a = (i - 1); a < (i + 2); a++){

              std::vector<XYPoint> ext_ring_graticule_cell =
                  find_graticule(old_hole[a][0], old_hole[a][1]);

              std::cout << "Point at (" << old_hole[a][0] << ", " << old_hole[a][1] << ")\n";

              std::cout << "On internal boundary of " << gd.id() << "\n";

              std::cout << "Graticule cell cordinates: \n";
              for (unsigned int z = 0; z < 4; z++){
                std::cout << "V" << z << ": (" << ext_ring_graticule_cell[z].x << ", " << ext_ring_graticule_cell[z].y << ")\n";
              }

              std::cout << "Diagonal chosen: " << graticule_diagonals[int(ext_ring_graticule_cell[0].x)][int(ext_ring_graticule_cell[0].y)] << "\n";
            }
          }
        }
      }
    }
  }
  return;
}