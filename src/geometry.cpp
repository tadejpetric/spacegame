#include "geometry.h"
#include <cmath>

void set_transform(GLuint program, float tx, float ty, float scaleX, float scaleY, float rotation) {
    float c = cos(rotation);
    float s = sin(rotation);
    
    float matrix[9] = {
        scaleX * c,  scaleX * s, 0.0f,
        -scaleY * s, scaleY * c, 0.0f,
        tx,          ty,         1.0f
    };
    
    GLint transformLoc = glGetUniformLocation(program, "transform");
    glUniformMatrix3fv(transformLoc, 1, GL_FALSE, matrix);
}

void draw_disc(GLuint vbo, float x, float y, float radius, float r, float g, float b, GLuint program, float aspect) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLint posAttrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    
    GLint colorUniform = glGetUniformLocation(program, "color");
    glUniform4f(colorUniform, r, g, b, 1.0f);
    
    set_transform(program, x, y, radius / aspect, radius);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 32);
}

void draw_triangle(GLuint vbo, float x, float y, float scale, float angle, float r, float g, float b, GLuint program, float aspect) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLint posAttrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    
    GLint colorUniform = glGetUniformLocation(program, "color");
    glUniform4f(colorUniform, r, g, b, 1.0f);
    
    set_transform(program, x, y, scale / aspect, scale, angle);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void draw_line(GLuint vbo, float x1, float y1, float x2, float y2, float r, float g, float b, GLuint program, float aspect) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrt(dx * dx + dy * dy);
    float angle = atan2(dy, dx);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLint posAttrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GLint colorUniform = glGetUniformLocation(program, "color");
    glUniform4f(colorUniform, r, g, b, 1.0f);

    set_transform(program, x1, y1, len, 1.0f, angle);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 2);
}
