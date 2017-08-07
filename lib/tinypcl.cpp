/*
** tiny Point Cloud Library
**
** Copyright (c) 2017 Jun Takeda
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#include <bitset>
#include <stdio.h>
#include <math.h>
#include "tinypcl.hpp"

// Constructor: Initializes PointCloud
PointCloud::PointCloud(void) {
    clear();
}

// Returns the value of the point
unsigned char PointCloud::get(unsigned int x, unsigned int y, unsigned int z) {
    return point_cloud_data(x,y,z);
}

// Sets the value of the point
void PointCloud::set(unsigned int x, unsigned int y, unsigned int z, unsigned char val) {
    point_cloud_data(x,y,z) = val;
}

// Clear all points
void PointCloud::clear(void) {
    for (int z=0;z<SIZE_Z;z++) {
        for (int y=0;y<SIZE_Y;y++) {
            for (int x=0;x<SIZE_X;x++) {
                point_cloud_data(x,y,z) = 1;
            }
        }
    }
}

// Compute normal
XYZ PointCloud::compute_normal(TRIANGLE triangle) {
    XYZ ab, bc;
    ab.x = triangle.p[1].x - triangle.p[0].x;
    ab.y = triangle.p[1].y - triangle.p[0].y;
    ab.z = triangle.p[1].z - triangle.p[0].z;
    bc.x = triangle.p[2].x - triangle.p[1].x;
    bc.y = triangle.p[2].y - triangle.p[1].y;
    bc.z = triangle.p[2].z - triangle.p[1].z;

    XYZ normal;
    normal.x = (ab.y * bc.z) - (ab.z * bc.y);
    normal.y = (ab.z * bc.x) - (ab.x * bc.z);
    normal.z = (ab.x * bc.y) - (ab.y * bc.x);

    double length = pow( ( normal.x * normal.x ) + ( normal.y * normal.y ) + ( normal.z * normal.z ), 0.5 );
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;

    return normal;
}

// Save 3D data as STL file with marching cubes algorithm
void PointCloud::save_as_stl(const char* file_name) {
    FILE *fp_stl = fopen(file_name, "wb");

    uint8_t header[80] = {0};
    uint32_t face_count = 0;
    uint16_t stub = 0;

    TRIANGLE triangles[5];
    GRIDCELL grid;

    // Write STL file header
    fwrite(header, sizeof(header), 1, fp_stl);
    fwrite(&face_count, sizeof(uint32_t), 1, fp_stl);
    
    // Write normal and vertex
    for (int z=0; z<SIZE_Z-1; z++) {
        printf("STL progress %d\r\n", z);
        for (int y=0; y<SIZE_Y-1; y++) {
            for (int x=0; x<SIZE_X-1; x++) {

                grid.p[0].x = x; 
                grid.p[0].y = y; 
                grid.p[0].z = z;
                grid.val[0] = point_cloud_data(x, y, z);

                grid.p[1].x = x+1;
                grid.p[1].y = y;
                grid.p[1].z = z;
                grid.val[1] = point_cloud_data(x+1, y, z);

                grid.p[2].x = x+1;
                grid.p[2].y = y+1;
                grid.p[2].z = z;
                grid.val[2] = point_cloud_data(x+1, y+1, z);

                grid.p[3].x = x;
                grid.p[3].y = y+1;
                grid.p[3].z = z;
                grid.val[3] = point_cloud_data(x, y+1, z);

                grid.p[4].x = x;
                grid.p[4].y = y;
                grid.p[4].z = z+1;
                grid.val[4] = point_cloud_data(x, y, z+1);

                grid.p[5].x = x+1;
                grid.p[5].y = y;
                grid.p[5].z = z+1;
                grid.val[5] = point_cloud_data(x+1, y, z+1);

                grid.p[6].x = x+1;
                grid.p[6].y = y+1;
                grid.p[6].z = z+1;
                grid.val[6] = point_cloud_data(x+1, y+1, z+1);
                
                grid.p[7].x = x;
                grid.p[7].y = y+1;
                grid.p[7].z = z+1;
                grid.val[7] = point_cloud_data(x, y+1, z+1);

                int ret = Polygonise(grid, 1, triangles);
                for (int i=0; i<ret; i++) {

                    XYZ normal = compute_normal(triangles[i]);
                    if (!isnan(normal.x)) {
                        // write Normal vector
                        fwrite(&normal, sizeof(XYZ), 1, fp_stl);

                        // write Vertex
                        for (int j=0;j<3;j++) {
                            triangles[i].p[j].x *= SCALE_X;
                            triangles[i].p[j].y *= SCALE_Y;
                            triangles[i].p[j].z *= SCALE_Z;
                            fwrite(&triangles[i].p[j], sizeof(XYZ), 1, fp_stl);
                        }

                        // write unused area
                        fwrite(&stub, sizeof(uint16_t), 1, fp_stl);

                        face_count++;
                    }
                }
            }
        }
    }
    // Write number of triangles
    fseek(fp_stl, 80, SEEK_SET);
    fwrite(&face_count, sizeof(uint32_t), 1, fp_stl);

    fclose(fp_stl);
}
