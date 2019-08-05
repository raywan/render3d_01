#include "mesh.h"
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <string>
#include <rw_math.h>
#include <assert.h>
#include <set>

using namespace std;

vector<string> split_face_element(const string &element, FaceFormat &assumed_ff, FaceFormat &cur_ff) {
  vector<string> result;
  stringstream ss(element);
  string token;

  while (getline(ss, token, '/')) {
    result.push_back(token);
  }

  if (result.size() == 3 && result[1] == "") { // v//vn
    cur_ff = FaceFormat::VVN;
  } else if (result.size() == 3) { // v/vt/vn
    cur_ff = FaceFormat::VVTVN;
  } else if (result.size() == 2) { // v/vt
    cur_ff = FaceFormat::VVT;
  } else if (result.size() == 1) { // v
    cur_ff = FaceFormat::V;
  }

  if (assumed_ff == FaceFormat::UNDEFINED) {
    assumed_ff = cur_ff;
  }

  return result;
}

static void pack_and_order_data(Mesh *out_mesh) {
  // Preallocate space for the packed array
  out_mesh->packed = (float *) malloc(sizeof(float) * NUM_PACKED_ELEMENTS * out_mesh->v.size());

  // Make sure that all the indices are the same length (they should be)
  // NOTE(ray): Not sure if I want to return an error code
  if (out_mesh->v_idx.size() > 0 && out_mesh->uv_idx.size() > 0 && out_mesh->n_idx.size() > 0) {
    assert(out_mesh->v_idx.size() == out_mesh->uv_idx.size() && out_mesh->v_idx.size() == out_mesh->n_idx.size());
    // Pack the vertex data
    for (int i = 0; i < out_mesh->v_idx.size(); ++i) {
      memcpy(out_mesh->packed + (NUM_PACKED_ELEMENTS * out_mesh->v_idx[i]),
          out_mesh->v.data() + out_mesh->v_idx[i], sizeof(Vec3));
      memcpy(out_mesh->packed + (NUM_PACKED_ELEMENTS * out_mesh->v_idx[i]) + 3,
          out_mesh->uv.data() + out_mesh->uv_idx[i], sizeof(Vec2));
      memcpy(out_mesh->packed + (NUM_PACKED_ELEMENTS * out_mesh->v_idx[i]) + 5,
          out_mesh->n.data() + out_mesh->n_idx[i], sizeof(Vec3));
    }

    // Loop through the temporary buffers
    for (int i = 0; i < out_mesh->v_idx.size(); ++i) {
      out_mesh->buf_v.push_back(out_mesh->v[out_mesh->v_idx[i]]);
      out_mesh->buf_uv.push_back(out_mesh->uv[out_mesh->uv_idx[i]]);
      out_mesh->buf_n.push_back(out_mesh->n[out_mesh->n_idx[i]]);
    }
  } else {
    // Pack the vertex data
    for (int i = 0; i < out_mesh->v_idx.size(); ++i) {
      memcpy(out_mesh->packed + (NUM_PACKED_ELEMENTS * out_mesh->v_idx[i]),
          out_mesh->v.data() + out_mesh->v_idx[i], sizeof(Vec3));
    }

    // Loop through the temporary buffers
    for (int i = 0; i < out_mesh->v_idx.size(); ++i) {
      out_mesh->buf_v.push_back(out_mesh->v[out_mesh->v_idx[i]]);
    }
  }
}

// NOTE(ray): We're storing a lot more data than necessary for debug reasons
uint8_t load_obj(Mesh *out_mesh, const char *obj_path) {
  cout << "Loading obj: " << obj_path << endl;

  FaceFormat assumed_ff = FaceFormat::UNDEFINED;
  FaceFormat cur_ff = FaceFormat::UNDEFINED;

  vector<int> v_idx;
  vector<int> uv_idx;
  vector<int> n_idx;

  // Read the file
  ifstream obj_file;
  obj_file.open(obj_path);

  if (!obj_file.is_open()) {
    cout << "Error reading file: " << obj_path << endl;
    return -1;
  }

  string line;
  float x, y, z;
  while (getline(obj_file, line)) {
    string s;
    istringstream iss(line);
    iss >> s;
    if (s == "#") {
      continue;
    } else if (s == "v") {
      iss >> x >> y >> z;
      if (iss.fail()) {
        iss.clear();
        //puts("V FAILED");
        return -2;
      }
      out_mesh->v.push_back({{x, y, z}});
    } else if (s == "vt") {
      iss >> x >> y;
      if (iss.fail()) {
        iss.clear();
        //puts("uv FAILED");
        return -3;
      }
      // NOTE(ray): There's an optional w value
      // Should be vec2 for optimal packing but just make it vec3 for now
      out_mesh->uv.push_back({{x, y}});
    } else if (s == "vn") {
      iss >> x >> y >> z;
      if (iss.fail()) {
        iss.clear();
        //puts("n FAILED");
        return -4;
      }
      out_mesh->n.push_back({{x, y, z}});
    } else if (s == "f") {
      // NOTE(ray): Possible formats: v, v//vn, v/vt, v/vt/vn
      int num_pushed = 0;
      while (!iss.eof()) {
        iss >> s;
        vector<string> face_components = split_face_element(s, assumed_ff, cur_ff);

        if (assumed_ff != cur_ff) {
          //puts("f failed");
          return -5;
        }

        ++num_pushed;

        switch (assumed_ff) {
          case FaceFormat::V:
            out_mesh->v_idx.push_back(stoi(face_components[0])-1);
            break;
          case FaceFormat::VVT:
            out_mesh->v_idx.push_back(stoi(face_components[0])-1);
            out_mesh->uv_idx.push_back(stoi(face_components[1])-1);
            break;
          case FaceFormat::VVN:
            out_mesh->v_idx.push_back(stoi(face_components[0])-1);
            out_mesh->n_idx.push_back(stoi(face_components[2])-1);
            break;
          case FaceFormat::VVTVN:
            out_mesh->v_idx.push_back(stoi(face_components[0])-1);
            out_mesh->uv_idx.push_back(stoi(face_components[1])-1);
            out_mesh->n_idx.push_back(stoi(face_components[2])-1);
            break;
          default:
            // This should NEVER happen
            return -69;
        }
}

      // We want to know how many vertices that a face has
      // NOTE(ray): We don't do anything with this data yet
      out_mesh->f.push_back(num_pushed);

      if (iss.fail()) {
        iss.clear();
      }
    }
  }

  pack_and_order_data(out_mesh);

  out_mesh->format = cur_ff;

  obj_file.close();
  return 0;
}