#include "constants.h"
#include "cartogram_info.h"
#include "inset_state.h"

void rescale_map(unsigned int long_grid_side_length,
                 InsetState *inset_state,
                 bool is_world_map)
{
  double padding = (is_world_map ?  1.0 : padding_unless_world);
  Bbox bb = inset_state->bbox();

  // Expand bounding box to guarantee a minimum padding
  double new_xmin =
    0.5 * ((1.0-padding)*bb.xmax() + (1.0+padding)*bb.xmin());
  double new_xmax =
    0.5 * ((1.0+padding)*bb.xmax() + (1.0-padding)*bb.xmin());
  double new_ymin =
    0.5 * ((1.0-padding)*bb.ymax() + (1.0+padding)*bb.ymin());
  double new_ymax =
    0.5 * ((1.0+padding)*bb.ymax() + (1.0-padding)*bb.ymin());

  // Ensure that the grid dimensions lx and ly are integer powers of 2
  if ((long_grid_side_length <= 0) ||
      ((long_grid_side_length &
        (~long_grid_side_length + 1)) != long_grid_side_length)) {
    std::cerr << "ERROR: long_grid_side_length must be an integer"
              << "power of 2." << std::endl;
    _Exit(15);
  }
  unsigned int lx, ly;
  double latt_const;
  if (bb.xmax()-bb.xmin() > bb.ymax()-bb.ymin()) {
    lx = long_grid_side_length;
    latt_const = (new_xmax-new_xmin) / lx;
    ly = 1 << static_cast<int>(ceil(log2((new_ymax-new_ymin) / latt_const)));
    new_ymax = 0.5*(bb.ymax()+bb.ymin()) + 0.5*ly*latt_const;
    new_ymin = 0.5*(bb.ymax()+bb.ymin()) - 0.5*ly*latt_const;
  } else {
    ly = long_grid_side_length;
    latt_const = (new_ymax-new_ymin) / ly;
    lx = 1 << static_cast<int>(ceil(log2((new_xmax-new_xmin) / latt_const)));
    new_xmax = 0.5*(bb.xmax()+bb.xmin()) + 0.5*lx*latt_const;
    new_xmin = 0.5*(bb.xmax()+bb.xmin()) - 0.5*lx*latt_const;
  }
  std::cerr << "Rescaling to " << lx << "-by-" << ly
            << " grid with bounding box" << std::endl;
  std::cerr << "\t("
            << new_xmin << ", " << new_ymin << ", "
            << new_xmax << ", " << new_ymax << ")"
            << std::endl;
  inset_state->set_grid_dimensions(lx, ly);

  // Rescale and translate all GeoDiv coordinates
  Transformation translate(CGAL::TRANSLATION,
                           CGAL::Vector_2<Epick>(-new_xmin, -new_ymin));
  Transformation scale(CGAL::SCALING, (1.0/latt_const));
  for (auto &gd : *inset_state->ref_to_geo_divs()) {
    for (auto &pwh : *gd.ref_to_polygons_with_holes()) {
      Polygon *ext_ring = &pwh.outer_boundary();
      *ext_ring = transform(translate, *ext_ring);
      *ext_ring = transform(scale, *ext_ring);
      for (auto hi = pwh.holes_begin(); hi != pwh.holes_end(); ++hi) {
        *hi = transform(translate, *hi);
        *hi = transform(scale, *hi);
      }
    }
  }

  // Storing coordinates to rescale in future
  inset_state->set_xmin(new_xmin);
  inset_state->set_ymin(new_ymin);
  inset_state->set_map_scale(latt_const);
  return;
}

void normalize_inset_area(InsetState *inset_state,
                          double total_cart_target_area,
                          bool equal_area)
{
  Bbox bb = inset_state->bbox();

  // Calculate scale_factor value to make insets proportional to each other
  double inset_size_proportion =
    inset_state->total_target_area() / total_cart_target_area;
  double scale_factor =
    equal_area ?
    1.0 :
    sqrt(inset_size_proportion / inset_state->total_inset_area());

  // Rescale and translate all GeoDiv coordinates
  Transformation translate(
    CGAL::TRANSLATION,
    CGAL::Vector_2<Epick>(-(bb.xmin() + bb.xmax()) / 2,
                          -(bb.ymin() + bb.ymax()) / 2)
    );
  Transformation scale(CGAL::SCALING, scale_factor);
  for (auto &gd : *inset_state->ref_to_geo_divs()) {
    for (auto &pwh : *gd.ref_to_polygons_with_holes()) {
      Polygon *ext_ring = &pwh.outer_boundary();
      *ext_ring = transform(translate, *ext_ring);
      *ext_ring = transform(scale, *ext_ring);
      for (auto hi = pwh.holes_begin(); hi != pwh.holes_end(); ++hi) {
        *hi = transform(translate, *hi);
        *hi = transform(scale, *hi);
      }
    }
  }
  return;
}

void shift_insets_to_target_position(CartogramInfo *cart_info)
{

  // For simplicity's sake, let us formally insert bounding boxes for
  // all conceivable inset positions
  std::map<std::string, Bbox> bboxes;
  std::string possible_inset_positions[5] = {"C", "B", "L", "T", "R"};
  for (auto pos : possible_inset_positions) {
    bboxes.insert(std::pair<std::string, Bbox>(pos, {0, 0, 0, 0}));
  }

  // If the inset actually exists, we get its current bounding box
  for (auto &[inset_pos, inset_state] : *cart_info->ref_to_inset_states()) {
    bboxes.at(inset_pos) = inset_state.bbox();
  }

  // Calculate the width and height of all positioned insets without spacing
  double width = bboxes.at("C").xmax() - bboxes.at("C").xmin() +
                 bboxes.at("L").xmax() - bboxes.at("L").xmin() +
                 bboxes.at("R").xmax() - bboxes.at("R").xmin();

  // Considering edge cases where width of "T" or "B" inset might be greater
  // than width of "C", "L", "R" insets combined
  width = std::max({
     bboxes.at("T").xmax() - bboxes.at("T").xmin(), // width of inset T
     bboxes.at("B").xmax() - bboxes.at("B").xmin(), // width of inset B
     width // width of inset C + L + R
   });

  double height = bboxes.at("C").ymax() - bboxes.at("C").ymin() +
                  bboxes.at("T").ymax() - bboxes.at("T").ymin() +
                  bboxes.at("B").ymax() - bboxes.at("B").ymin();

  // Considering edge cases where height of "L" or "R" inset might be greater
  // than height of "T", "C", "R" insets combined
  height = std::max({
    bboxes.at("R").ymax() - bboxes.at("R").ymin(), // height of inset R
    bboxes.at("L").ymax() - bboxes.at("L").ymin(), // height of inset L
    height // height of inset C + T + B
  });

  // Spacing between insets
  double inset_spacing = std::max(width, height) * inset_spacing_factor;
  for (auto &[inset_pos, inset_state] : *cart_info->ref_to_inset_states()) {

    // Assuming X and Y value of translation vector to be 0 to begin with
    double x = 0;
    double y = 0;
    const std::string pos = inset_pos;

    // We only need to modify either X or Y, depening on the inset_pos
    if (pos == "R") {
      x = std::max({bboxes.at("C").xmax(),
                    bboxes.at("B").xmax(),
                    bboxes.at("T").xmax()});
      x += bboxes.at("R").xmax();
      x += inset_spacing;
    } else if (pos == "L") {
      x = std::min({bboxes.at("C").xmin(),
                    bboxes.at("B").xmin(),
                    bboxes.at("T").xmin()});

      // Over here, xmin is negative and lies in the 2nd and 3rd quadrant
      x += bboxes.at("L").xmin();
      x -= inset_spacing;
    } else if (pos == "T") {
      y = std::max({bboxes.at("C").ymax(),
                    bboxes.at("R").ymax(),
                    bboxes.at("L").ymax()});
      y += bboxes.at("T").ymax();
      y += inset_spacing;
    } else if (pos == "B") {
      y = std::min({bboxes.at("C").ymin(),
                    bboxes.at("R").ymin(),
                    bboxes.at("L").ymin()});

      // Over here, ymin is negative and lies in the 3th and 4th quadrant
      y += bboxes.at("B").ymin();
      y -= inset_spacing;
    }

    // Translating inset according to translation vector calculated above
    Transformation translate(CGAL::TRANSLATION,
                             CGAL::Vector_2<Epick>(x, y));
    for (auto &gd : *inset_state.ref_to_geo_divs()) {
      for (auto &pwh : *gd.ref_to_polygons_with_holes()) {
        Polygon *ext_ring = &pwh.outer_boundary();
        *ext_ring = transform(translate, *ext_ring);
        for (auto hi = pwh.holes_begin(); hi != pwh.holes_end(); ++hi) {
          *hi = transform(translate, *hi);
        }
      }
    }
  }
  return;
}