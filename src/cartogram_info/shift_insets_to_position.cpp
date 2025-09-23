#include "cartogram_info.hpp"
#include "constants.hpp"

void CartogramInfo::reposition_insets(bool output_to_stdout)
{

  // Warn user about repositoning insets with `--skip_projection` flag
  if (args_.skip_projection && n_insets() > 1) {
    std::cerr << "WARNING: Trying to repostion insets with ";
    if (is_projected_) {
      std::cerr << "original input map already projected. ";
    } else {
      std::cerr << "`--skip_projection` flag present. ";
    }
    std::cerr << "This implies that map has already been projected with "
              << "standard parallels based on original, unprojected map. "
              << "Insets may appear skewed. " << std::endl;
  }

  // For simplicity's sake, let us formally insert bounding boxes for
  // all conceivable inset positions
  std::map<std::string, Bbox> bboxes;
  std::string possible_inset_positions[5] = {"C", "B", "L", "T", "R"};
  for (const auto &pos : possible_inset_positions) {
    bboxes.insert(std::pair<std::string, Bbox>(pos, {0, 0, 0, 0}));
  }

  // If the inset actually exists, we get its current bounding box
  for (const InsetState &inset_state : inset_states_) {
    std::string inset_pos = inset_state.pos();
    bboxes.at(inset_pos) = inset_state.bbox(output_to_stdout);
  }

  const double height_C = bboxes.at("C").ymax() - bboxes.at("C").ymin();
  const double width_C = bboxes.at("C").xmax() - bboxes.at("C").xmin();

  // Spacing between insets
  for (InsetState &inset_state : inset_states_) {
    std::string inset_pos = inset_state.pos();

    // Assuming X and Y value of translation vector to be 0 to begin with
    double x = 0;
    double y = 0;
    const std::string pos = inset_pos;

    // We only need to modify either x-coordinates or y-coordinates, depending
    // on the inset position
    if (pos == "R") {
      x = std::max(
        {bboxes.at("C").xmax(), bboxes.at("B").xmax(), bboxes.at("T").xmax()});
      x += bboxes.at("R").xmax();

      const double width_R = bboxes.at("R").xmax() - bboxes.at("R").xmin();
      const double inset_spacing = (width_C + width_R) * inset_spacing_factor;

      x += inset_spacing;
    } else if (pos == "L") {
      x = std::min(
        {bboxes.at("C").xmin(), bboxes.at("B").xmin(), bboxes.at("T").xmin()});

      // At "L", xmin is negative and lies in the 2nd and 3rd quadrant
      x += bboxes.at("L").xmin();

      const double width_L = bboxes.at("L").xmax() - bboxes.at("L").xmin();
      const double inset_spacing = (width_C + width_L) * inset_spacing_factor;

      x -= inset_spacing;
    } else if (pos == "T") {
      y = std::max(
        {bboxes.at("C").ymax(), bboxes.at("R").ymax(), bboxes.at("L").ymax()});
      y += bboxes.at("T").ymax();

      const double height_T = bboxes.at("T").ymax() - bboxes.at("T").ymin();
      const double inset_spacing =
        (height_C + height_T) * inset_spacing_factor;

      y += inset_spacing;
    } else if (pos == "B") {
      y = std::min(
        {bboxes.at("C").ymin(), bboxes.at("R").ymin(), bboxes.at("L").ymin()});

      // At "B", ymin is negative and lies in the 3rd and 4th quadrant
      y += bboxes.at("B").ymin();

      const double height_B = bboxes.at("B").ymax() - bboxes.at("B").ymin();
      const double inset_spacing =
        (height_C + height_B) * inset_spacing_factor;

      y -= inset_spacing;
    }

    // Translating inset according to translation vector calculated above
    const Transformation translate(
      CGAL::TRANSLATION,
      CGAL::Vector_2<Scd>(x, y));

    // Apply translation to all points
    inset_state.transform_points(translate, false);

    if (output_to_stdout) {
      inset_state.transform_points(translate, true);
    }
  }
}
