#pragma once
#include "app.hpp"
#include "utils.hpp"

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <charconv>

namespace flb
{
static MeshData loadOBJ(const std::filesystem::path& path)
{
  const auto content = readFile(path);

  size_t vCount = 0;
  size_t vnCount = 0;
  size_t vtCount = 0;

  std::stringstream countStream(content);
  std::string line;
  while (std::getline(countStream, line)) {
    if (line.starts_with("v ")) {
      vCount++;
    } else if (line.starts_with("vn ")) {
      vnCount++;
    } else if (line.starts_with("vt ")) {
      vtCount++;
    }
  }

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> texCoords;
  std::vector<glm::vec3> colors;

  positions.reserve(vCount);
  colors.reserve(vCount);
  normals.reserve(vnCount);
  texCoords.reserve(vtCount);

  std::vector<Vertex> vertices;
  std::vector<Index> indices;
  std::map<std::tuple<int, int, int>, Index> uniqueVertices;

  std::stringstream ss(content);
  while (std::getline(ss, line)) {
    if (line.starts_with("v ")) {
      std::stringstream ls(line.substr(2));
      glm::vec3 v;
      ls >> v.x >> v.y >> v.z;
      positions.push_back(v);

      glm::vec3 c(1.0f);
      if (!(ls >> c.r >> c.g >> c.b)) {
        c = glm::vec3(1.0f);
      }
      colors.push_back(c);
    } else if (line.starts_with("vn ")) {
      std::stringstream ls(line.substr(3));
      glm::vec3 vn;
      ls >> vn.x >> vn.y >> vn.z;
      normals.push_back(vn);
    } else if (line.starts_with("vt ")) {
      std::stringstream ls(line.substr(3));
      glm::vec2 vt;
      ls >> vt.x >> vt.y;
      texCoords.push_back(vt);
    } else if (line.starts_with("f ")) {
      std::stringstream ls(line.substr(2));
      std::string segment;
      std::vector<std::tuple<int, int, int>> faceIndices;

      while (ls >> segment) {
        int vIdx = -1, vtIdx = -1, vnIdx = -1;

        size_t firstSlash = segment.find('/');
        size_t secondSlash = segment.find('/', firstSlash + 1);

        if (firstSlash == std::string::npos) {
          std::from_chars(segment.data(), segment.data() + segment.size(), vIdx);
        } else {
          std::from_chars(segment.data(), segment.data() + firstSlash, vIdx);

          if (secondSlash == std::string::npos) {
            // v/vt
            std::from_chars(segment.data() + firstSlash + 1, segment.data() + segment.size(), vtIdx);
          } else {
            // v/vt/vn or v//vn
            if (secondSlash > firstSlash + 1) {
              std::from_chars(segment.data() + firstSlash + 1, segment.data() + secondSlash, vtIdx);
            }
            std::from_chars(segment.data() + secondSlash + 1, segment.data() + segment.size(), vnIdx);
          }
        }
        // Adjust 1-based to 0-based
        if (vIdx > 0) vIdx--;
        if (vtIdx > 0) vtIdx--;
        if (vnIdx > 0) vnIdx--;

        faceIndices.emplace_back(vIdx, vtIdx, vnIdx);
      }

      // Triangulate (Fan)
      for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
        std::tuple<int, int, int> triVerts[3] = {
            faceIndices[0],
            faceIndices[i],
            faceIndices[i + 1]
        };

        for (const auto& key : triVerts) {
          if (uniqueVertices.find(key) == uniqueVertices.end()) {
             Vertex vertex{};
             auto [v, vt, vn] = key;

             if (v >= 0 && v < positions.size()) vertex.position = positions[v];
             if (v >= 0 && v < colors.size()) vertex.color = colors[v]; else vertex.color = {1.0f, 1.0f, 1.0f};
             if (vn >= 0 && vn < normals.size()) vertex.normal = normals[vn];

             uniqueVertices[key] = static_cast<Index>(vertices.size());
             vertices.push_back(vertex);
          }
          indices.push_back(uniqueVertices[key]);
        }
      }
    }
  }

  return { vertices, indices };
}
} // namespace flb
