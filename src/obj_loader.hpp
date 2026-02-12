#pragma once
#include "app.hpp"
#include "utils.hpp"

#include <charconv>
#include <vector>
#include <tuple>
#include <map>
#include <filesystem>


namespace
{
  inline const char* skipSpace(const char* ptr, const char* end)
  {
    while (ptr < end && (*ptr == ' ' || *ptr == '\t')) {
      ptr++;
    }
    return ptr;
  }

  inline const char* nextLine(const char* ptr, const char* end)
  {
    while (ptr < end && *ptr != '\n' && *ptr != '\r') {
      ptr++;
    }
    // Skip potential \r\n or just \n
    if (ptr < end && *ptr == '\r') ptr++;
    if (ptr < end && *ptr == '\n') ptr++;
    return ptr;
  }

  inline const char* parseFloat(const char* ptr, const char* end, float& value)
  {
    ptr = skipSpace(ptr, end);
    auto res = std::from_chars(ptr, end, value);
    return res.ptr;
  }

  inline const char* parseInt(const char* ptr, const char* end, int& value)
  {
    auto res = std::from_chars(ptr, end, value);
    return res.ptr;
  }
}

namespace flb
{
  static MeshData loadOBJ(const std::filesystem::path& path)
  {
    // Read file into memory (This is the only necessary allocation)
    const auto content = readFile(path);

    // Pointers to traverse the buffer
    const char* ptr = content.data();
    const char* end = content.data() + content.size();

    size_t vCount = 0;
    size_t vnCount = 0;
    size_t vtCount = 0;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> colors;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    std::map<std::tuple<int, int, int>, Index> uniqueVertices;

    // Parse
    while (ptr < end) {
      // Fast skip of empty lines or comments
      ptr = skipSpace(ptr, end);
      if (ptr >= end) break;

      if (*ptr == '#') {
        ptr = nextLine(ptr, end);
        continue;
      }

      // Identify line type based on first two characters
      if (*ptr == 'v') {
        char type = (ptr + 1 < end) ? *(ptr + 1) : '\0';

        if (type == ' ') {
          // "v ": Vertex Position + optional color
          ptr += 2; // Skip "v "
          glm::vec3 v;
          ptr = parseFloat(ptr, end, v.x);
          ptr = parseFloat(ptr, end, v.y);
          ptr = parseFloat(ptr, end, v.z);
          positions.push_back(v);

          // Check for vertex colors
          // Scan ahead to see if there is more data before the newline
          const char* lineEnd = ptr;
          while(lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;

          const char* check = skipSpace(ptr, lineEnd);
          if (check < lineEnd) {
            glm::vec3 c;
            ptr = parseFloat(ptr, end, c.r);
            ptr = parseFloat(ptr, end, c.g);
            ptr = parseFloat(ptr, end, c.b);
            colors.push_back(c);
          } else {
            colors.emplace_back(1.0f);
          }

        } else if (type == 'n') {
          // "vn ": Normal
          ptr += 3; // Skip "vn "
          glm::vec3 vn;
          ptr = parseFloat(ptr, end, vn.x);
          ptr = parseFloat(ptr, end, vn.y);
          ptr = parseFloat(ptr, end, vn.z);
          normals.push_back(vn);

        } else if (type == 't') {
          // "vt ": Texture Coordinate
          ptr += 3; // Skip "vt "
          glm::vec2 vt;
          ptr = parseFloat(ptr, end, vt.x);
          ptr = parseFloat(ptr, end, vt.y);
          texCoords.push_back(vt);
        } else {
          // Unknown "vX" format, skip
        }

        ptr = nextLine(ptr, end);

      } else if (*ptr == 'f' && (ptr + 1 < end && *(ptr + 1) == ' ')) {
        // "f ": Faces
        ptr += 2; // Skip "f "
        const char* lineEnd = ptr;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;

        std::vector<std::tuple<int, int, int>> faceIndices;

        while (ptr < lineEnd) {
          ptr = skipSpace(ptr, lineEnd);
          if (ptr >= lineEnd) break;

          int vIdx = -1, vtIdx = -1, vnIdx = -1;

          // Parse v
          ptr = parseInt(ptr, lineEnd, vIdx);

          // Check for /
          if (ptr < lineEnd && *ptr == '/') {
            ptr++; // Skip first /

            if (ptr < lineEnd && *ptr == '/') {
              // Case: v//vn
              ptr++; // Skip second /
              ptr = parseInt(ptr, lineEnd, vnIdx);
            } else {
              // Case: v/vt...
              ptr = parseInt(ptr, lineEnd, vtIdx);
              if (ptr < lineEnd && *ptr == '/') {
                // Case: v/vt/vn
                ptr++; // Skip second /
                ptr = parseInt(ptr, lineEnd, vnIdx);
              }
            }
          }

          // Adjust 1-based to 0-based
          if (vIdx > 0) vIdx--;
          if (vtIdx > 0) vtIdx--;
          if (vnIdx > 0) vnIdx--;

          faceIndices.emplace_back(vIdx, vtIdx, vnIdx);
        }

        // Triangulate (Fan)
        // Note: This logic remains the same, but now operates on stack data
        // without string parsing overhead.
        for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
          std::tuple<int, int, int> triVerts[3] = {
            faceIndices[0],
            faceIndices[i],
            faceIndices[i + 1]
          };

          for (const auto& key : triVerts) {
            auto it = uniqueVertices.find(key);
            if (it == uniqueVertices.end()) {
              Vertex vertex{};
              auto [v, vt, vn] = key;

              if (v >= 0 && v < positions.size()) vertex.position = positions[v];
              if (v >= 0 && v < colors.size()) vertex.color = colors[v];
              else vertex.color = {1.0f, 1.0f, 1.0f};

              // Handle texture coordinates
              if (vt >= 0 && vt < texCoords.size()) vertex.uv = texCoords[vt];

              if (vn >= 0 && vn < normals.size()) vertex.normal = normals[vn];

              Index newIndex = static_cast<Index>(vertices.size());
              uniqueVertices[key] = newIndex;
              vertices.push_back(vertex);
              indices.push_back(newIndex);
            } else {
              indices.push_back(it->second);
            }
          }
        }

        // Move to next line (we already found lineEnd, but let's safely advance)
        ptr = nextLine(lineEnd, end);
      } else {
        // Unknown line type, just skip
        ptr = nextLine(ptr, end);
      }
    }

    return { vertices, indices };
  }
} // namespace flb
