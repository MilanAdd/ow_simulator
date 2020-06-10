#include <OgreVector3.h>
#include <sensor_msgs/image_encodings.h>
#include <gazebo/common/Console.hh>
#include "OpenCV_Util.h"
#include "TerrainBrush.h"
#include "TerrainModifier.h"
#include "MergeOperations.h"

using namespace std;
using namespace Ogre;
using namespace gazebo;
using namespace rendering;
using namespace geometry_msgs;
using namespace sensor_msgs;
using namespace cv;
using namespace cv_bridge;
using ignition::math::clamp;
using namespace ow_dynamic_terrain;

void TerrainModifier::modifyCircle(Heightmap* heightmap, const modify_terrain_circle::ConstPtr& msg,
                                   function<float(long, long)> get_height_value,
                                   function<void(long, long, float)> set_height_value)
{
  if (msg->outer_radius <= 0.0f)
  {
    gzerr << "DynamicTerrain: outer_radius has to be a positive number!" << endl;
    return;
  }
  if (msg->inner_radius > msg->outer_radius)
  {
    gzerr << "DynamicTerrain: inner_radius can't exceed outer_radius value!" << endl;
    return;
  }

  if (heightmap == nullptr)
  {
    gzerr << "DynamicTerrain: heightmap is null!" << endl;
    return;
  }

  auto terrain = heightmap->OgreTerrain()->getTerrain(0, 0);

  if (!terrain)
  {
    gzerr << "DynamicTerrain: Heightmap has no associated terrain object!" << endl;
    return;
  }

  auto _terrain_position = Vector3(msg->position.x, msg->position.y, 0);
  auto heightmap_position = Vector3();
  terrain->getTerrainPosition(_terrain_position, &heightmap_position);
  auto heightmap_size = static_cast<int>(terrain->getSize());
  auto image =
      TerrainBrush::circle(heightmap_size * msg->outer_radius, heightmap_size * msg->inner_radius, msg->weight);
  auto center = Point2i(lround(heightmap_size * heightmap_position.x), lround(heightmap_size * heightmap_position.y));
  applyImageToHeightmap(heightmap, center, image, get_height_value, set_height_value, MergeOperations::min);

  gzlog << "DynamicTerrain: circle operation performed at (" << msg->position.x << ", " << msg->position.y << ")"
        << endl;
}

void TerrainModifier::modifyEllipse(Heightmap* heightmap, const modify_terrain_ellipse::ConstPtr& msg,
                                    function<float(long, long)> get_height_value,
                                    function<void(long, long, float)> set_height_value)
{
  if (msg->outer_radius_a <= 0.0f || msg->outer_radius_b <= 0.0f)
  {
    gzerr << "DynamicTerrain: outer_radius a & b has to be positive!" << endl;
    return;
  }

  if (msg->inner_radius_a > msg->outer_radius_a || msg->inner_radius_b > msg->outer_radius_b)
  {
    gzerr << "DynamicTerrain: inner_radius can't exceed outer_radius value!" << endl;
    return;
  }

  if (heightmap == nullptr)
  {
    gzerr << "DynamicTerrain: heightmap is null!" << endl;
    return;
  }

  auto terrain = heightmap->OgreTerrain()->getTerrain(0, 0);

  if (!terrain)
  {
    gzerr << "DynamicTerrain: Heightmap has no associated terrain object!" << endl;
    return;
  }

  auto _terrain_position = Vector3(msg->position.x, msg->position.y, 0);
  auto heightmap_position = Vector3();
  terrain->getTerrainPosition(_terrain_position, &heightmap_position);
  auto heightmap_size = static_cast<int>(terrain->getSize());
  auto center = Point2i(lround(heightmap_size * heightmap_position.x), lround(heightmap_size * heightmap_position.y));

  auto image =
      TerrainBrush::ellipse(heightmap_size * msg->outer_radius_a, heightmap_size * msg->inner_radius_a,
                            heightmap_size * msg->outer_radius_b, heightmap_size * msg->inner_radius_b, msg->weight);
  image = OpenCV_Util::expandImage(image);  // expand the image to hold rotation output with no loss
  image = OpenCV_Util::rotateImage(image, msg->orientation);

  applyImageToHeightmap(heightmap, center, image, get_height_value, set_height_value, MergeOperations::min);

  gzlog << "DynamicTerrain: ellipse operation performed at (" << msg->position.x << ", " << msg->position.y << ")"
        << endl;
}

void TerrainModifier::modifyPatch(Heightmap* heightmap, const modify_terrain_patch::ConstPtr& msg,
                                  function<float(long, long)> get_height_value,
                                  function<void(long, long, float)> set_height_value)
{
  if (heightmap == nullptr)
  {
    gzerr << "DynamicTerrain: heightmap is null!" << endl;
    return;
  }

  auto terrain = heightmap->OgreTerrain()->getTerrain(0, 0);

  if (!terrain)
  {
    gzerr << "DynamicTerrain: Heightmap has no associated terrain object!" << endl;
    return;
  }

  if (msg->patch.encoding != "32FC1")
  {
    gzerr << "DynamicTerrain: Only 32FC1 formats are supported" << endl;
    return;
  }

  auto _terrain_position = Vector3(msg->position.x, msg->position.y, 0);
  auto heightmap_position = Vector3();
  terrain->getTerrainPosition(_terrain_position, &heightmap_position);
  auto heightmap_size = terrain->getSize();
  auto center = Point2i(lroundf(heightmap_size * heightmap_position.x), lroundf(heightmap_size * heightmap_position.y));

  auto image_handle = TerrainModifier::importImageToOpenCV(msg);
  if (image_handle == nullptr)
  {
    gzerr << "DynamicTerrain: Failed to convert ROS image" << endl;
    return;
  }
  auto image = OpenCV_Util::expandImage(image_handle->image);  // expand the image to hold rotation output with no loss
  image = OpenCV_Util::rotateImage(image, msg->orientation);
  
  applyImageToHeightmap(heightmap, center, image, get_height_value, set_height_value, MergeOperations::min);

  gzlog << "DynamicTerrain: patch applied at (" << msg->position.x << ", " << msg->position.y << ")" << endl;
}

CvImageConstPtr TerrainModifier::importImageToOpenCV(const modify_terrain_patch::ConstPtr& msg)
{
  CvImageConstPtr image_handle;

  try
  {
    image_handle = toCvShare(msg->patch, msg, image_encodings::TYPE_32FC1);  // Using single precision (32-bit) float
                                                                             // same as the heightmap
  }
  catch (cv_bridge::Exception& e)
  {
    gzerr << "DynamicTerrain: cv_bridge exception: %s" << e.what() << endl;
    return nullptr;
  }

  return image_handle;
}

void TerrainModifier::applyImageToHeightmap(Heightmap* heightmap, const Point2i& center, const Mat& image,
                                            std::function<float(long, long)> get_height_value,
                                            std::function<void(long, long, float)> set_height_value,
                                            std::function<float(float, float)> merge_operation)
{
  auto terrain = heightmap->OgreTerrain()->getTerrain(0, 0);

  if (!terrain)
  {
    gzerr << "DynamicTerrain: Heightmap has no associated terrain object!" << endl;
    return;
  }

  if (image.type() != CV_32FC1)
  {
    gzerr << "DynamicTerrain: Only 32FC1 formats are supported" << endl;
    return;
  }

  auto heightmap_size = static_cast<int>(terrain->getSize());
  auto left = max(center.x - image.cols / 2, 0);
  auto top = max(center.y - image.rows / 2, 0);
  auto right = min(center.x - image.cols / 2 + image.cols - 1, heightmap_size);
  auto bottom = min(center.y - image.rows / 2 + image.rows - 1, heightmap_size);

  for (auto y = top; y <= bottom; ++y)
    for (auto x = left; x <= right; ++x)
    {
      auto row = y - top;
      auto col = x - left;
      auto new_height = merge_operation(get_height_value(x, y), image.at<float>(row, col));
      set_height_value(x, y, new_height);
    }
}