// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include <cstdio>

#include "open3d/io/FileFormatIO.h"
#include "open3d/t/io/PointCloudIO.h"
#include "open3d/utility/Console.h"
#include "open3d/utility/FileSystem.h"
#include "open3d/utility/Helper.h"
#include "open3d/utility/ProgressReporters.h"

namespace open3d {
namespace t {
namespace io {

bool ReadPointCloudFromPTS(const std::string &filename,
                           geometry::PointCloud &pointcloud,
                           const ReadPointCloudOption &params) {
    try {
        utility::filesystem::CFile file;
        if (!file.Open(filename, "r")) {
            utility::LogWarning("Read PTS failed: unable to open file: {}",
                                filename);
            return false;
        }
        size_t num_of_pts = 0;
        const char *line_buffer;
        if ((line_buffer = file.ReadLine())) {
            sscanf(line_buffer, "%zu", &num_of_pts);
        }
        if (num_of_pts <= 0) {
            utility::LogWarning("Read PTS failed: unable to read header.");
            return false;
        }
        utility::CountingProgressReporter reporter(params.update_progress);
        reporter.SetTotal(num_of_pts);

        pointcloud.Clear();
        core::Tensor points;
        core::Tensor intensities;
        core::Tensor colors;
        double *points_ptr = NULL;
        double *intensities_ptr = NULL;
        uint8_t *colors_ptr = NULL;
        size_t idx = 0;
        std::vector<std::string> st;
        int num_of_fields = 0;

        while (idx < num_of_pts && (line_buffer = file.ReadLine())) {
            st.clear();
            utility::SplitString(st, line_buffer, " ");
            if (num_of_fields == 0) {
                num_of_fields = (int)st.size();
                if (num_of_fields < 3) {
                    utility::LogWarning(
                            "Read PTS failed: insufficient data fields.");
                    return false;
                }
                points = core::Tensor({(int64_t)num_of_pts, 3},
                                      core::Dtype::Float64);
                points_ptr = points.GetDataPtr<double>();

                // X Y Z I
                if (num_of_fields >= 4) {
                    intensities = core::Tensor({(int64_t)num_of_pts, 1},
                                               core::Dtype::Float64);
                    intensities_ptr = intensities.GetDataPtr<double>();
                }

                // X Y Z I R G B
                if (num_of_fields >= 7) {
                    colors = core::Tensor({(int64_t)num_of_pts, 3},
                                          core::Dtype::UInt8);
                    colors_ptr = colors.GetDataPtr<uint8_t>();
                }
            }

            if (num_of_fields > (int)st.size()) {
                utility::LogWarning(
                        "Read PTS failed: lines have unequal elements.");
                return false;
            }

            double x, y, z, i;
            int r, g, b;
            if (num_of_fields >= 7 &&
                (sscanf(line_buffer, "%lf %lf %lf %lf %d %d %d", &x, &y, &z, &i,
                        &r, &g, &b) == 7)) {
                points_ptr[3 * idx + 0] = x;
                points_ptr[3 * idx + 1] = y;
                points_ptr[3 * idx + 2] = z;
                intensities_ptr[idx] = i;
                colors_ptr[3 * idx + 0] = r;
                colors_ptr[3 * idx + 1] = g;
                colors_ptr[3 * idx + 2] = b;
            } else if (num_of_fields >= 4 &&
                       (sscanf(line_buffer, "%lf %lf %lf %lf", &x, &y, &z,
                               &i) == 4)) {
                points_ptr[3 * idx + 0] = x;
                points_ptr[3 * idx + 1] = y;
                points_ptr[3 * idx + 2] = z;
                intensities_ptr[idx] = i;
            } else if (sscanf(line_buffer, "%lf %lf %lf", &x, &y, &z) == 3) {
                points_ptr[3 * idx + 0] = x;
                points_ptr[3 * idx + 1] = y;
                points_ptr[3 * idx + 2] = z;
            }
            idx++;
            if (idx % 1000 == 0) {
                reporter.Update(idx);
            }
        }

        pointcloud.SetPoints(points);
        if (num_of_fields >= 4) {
            pointcloud.SetPointAttr("intensities", intensities);
        }
        if (num_of_fields >= 7) {
            pointcloud.SetPointColors(colors);
        }
        reporter.Finish();
        return true;
    } catch (const std::exception &e) {
        utility::LogWarning("Read PTS failed with exception: {}", e.what());
        return false;
    }
}

bool WritePointCloudToPTS(const std::string &filename,
                          const geometry::PointCloud &pointcloud,
                          const WritePointCloudOption &params) {
    try {
        utility::filesystem::CFile file;
        if (!file.Open(filename, "w")) {
            utility::LogWarning("Write PTS failed: unable to open file: {}",
                                filename);
            return false;
        }

        if (pointcloud.IsEmpty()) {
            utility::LogWarning("Write PTS failed: point cloud has 0 points.");
            return false;
        }

        utility::CountingProgressReporter reporter(params.update_progress);
        const core::Tensor &points = pointcloud.GetPoints();
        int64_t num_points = static_cast<long>(points.GetLength());

        core::Tensor colors;
        core::Tensor intensities;
        if (pointcloud.HasPointColors()) {
            colors = pointcloud.GetPointColors();
        }

        if (pointcloud.HasPointAttr("intensities")) {
            intensities = pointcloud.GetPointAttr("intensities");
        }

        reporter.SetTotal(num_points);

        if (fprintf(file.GetFILE(), "%zu\r\n", (size_t)num_points) < 0) {
            utility::LogWarning("Write PTS failed: unable to write file: {}",
                                filename);
            return false;
        }

        for (int i = 0; i < num_points; i++) {
            if (pointcloud.HasPointColors() &&
                pointcloud.HasPointAttr("intensities")) {
                if (fprintf(file.GetFILE(),
                            "%.10f %.10f %.10f %.10f %d %d %d\r\n",
                            points[i][0].Item<double>(),
                            points[i][1].Item<double>(),
                            points[i][2].Item<double>(),
                            intensities[i].Item<double>(),
                            colors[i][0].Item<uint8_t>(),
                            colors[i][1].Item<uint8_t>(),
                            colors[i][2].Item<uint8_t>()) < 0) {
                    utility::LogWarning(
                            "Write PTS failed: unable to write file: {}",
                            filename);
                    return false;
                }
            } else if (pointcloud.HasPointAttr("intensities")) {
                if (fprintf(file.GetFILE(), "%.10f %.10f %.10f %.10f\r\n",
                            points[i][0].Item<double>(),
                            points[i][1].Item<double>(),
                            points[i][2].Item<double>(),
                            intensities[i].Item<double>()) < 0) {
                    utility::LogWarning(
                            "Write PTS failed: unable to write file: {}",
                            filename);
                    return false;
                }
            } else {
                if (fprintf(file.GetFILE(), "%.10f %.10f %.10f\r\n",
                            points[i][0].Item<double>(),
                            points[i][1].Item<double>(),
                            points[i][2].Item<double>()) < 0) {
                    utility::LogWarning(
                            "Write PTS failed: unable to write file: {}",
                            filename);
                    return false;
                }
            }
            if (i % 1000 == 0) {
                reporter.Update(i);
            }
        }
        reporter.Finish();
        return true;
    } catch (const std::exception &e) {
        utility::LogWarning("Write PTS failed with exception: {}", e.what());
        return false;
    }
    return true;
}

}  // namespace io
}  // namespace t
}  // namespace open3d
