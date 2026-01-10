#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <SDL_opengles2.h>

void draw_disc(GLuint vbo, float x, float y, float radius, float r, float g, float b, GLuint program, float aspect);
void draw_triangle(GLuint vbo, float x, float y, float scale, float angle, float r, float g, float b, GLuint program, float aspect);
void draw_line(GLuint vbo, float x1, float y1, float x2, float y2, float r, float g, float b, GLuint program, float aspect);
void set_transform(GLuint program, float tx, float ty, float scaleX, float scaleY, float rotation = 0.0f);

#endif
