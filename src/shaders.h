#pragma once

const char *shader_vert_quad_src =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec2 aTexCoord;\n"
"layout (location = 2) in vec4 aColor;\n"
"layout(std140) uniform Matrices {\n"
"    mat4 u_mvp;\n"
"};\n"
"out vec2 TexCoord;\n"
"out vec4 Color;\n"
"void main() {\n"
"    gl_Position = u_mvp * vec4(aPos, 0.0, 1.0);\n"
"    TexCoord = aTexCoord;\n"
"    Color = aColor;\n"
"}\n";

const char *shader_vert_flipped_quad_src =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec2 aTexCoord;\n"
"layout (location = 2) in vec4 aColor;\n"
"layout(std140) uniform Matrices {\n"
"    mat4 u_mvp;\n"
"};\n"
"out vec2 TexCoord;\n"
"out vec4 Color;\n"
"void main() {\n"
"    gl_Position = u_mvp * vec4(aPos, 0.0, 1.0);\n"
"    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);\n"
"    Color = aColor;\n"
"}\n";

const char *shader_frag_quad_src =
"#version 330 core\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"in vec4 Color;\n"
"void main() {\n"
"    FragColor = Color;\n"
"}\n";

const char *shader_frag_tex_src =
"#version 330 core\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D u_tex;\n"
"in vec4 Color;\n"
"void main() {\n"
"    FragColor = Color * texture(u_tex, TexCoord);\n"
"}\n";

const char *shader_frag_font_src =
"#version 330 core\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D u_tex;\n"
"in vec4 Color;\n"
"void main() {\n"
"    FragColor = vec4(vec3(Color), Color.a * texture(u_tex, TexCoord).r);\n"
"}\n";

const char *shader_frag_grid_src =
"#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec2 u_resolution;\n"
"uniform vec2 u_offset;\n"
"uniform float u_spacing;\n"
"void main() {\n"
"    vec2 screenPos = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);"
"    vec2 canvasPos = screenPos + u_offset;\n"
"\n"
"    float distX = abs(mod(canvasPos.x, u_spacing));\n"
"    distX = min(distX, u_spacing - distX);\n"
"    float gridX = smoothstep(1.0, 0.0, distX);\n"
"    float distY = abs(mod(canvasPos.y, u_spacing));\n"
"    distY = min(distY, u_spacing - distY);\n"
"    float gridY = smoothstep(1.0, 0.0, distY);\n"
"\n"
"    float originX = smoothstep(2.0, 0.0, abs(canvasPos.x));\n"
"    float originY = smoothstep(2.0, 0.0, abs(canvasPos.y));\n"
"\n"
"    vec3 gridColor = mix(vec3(0.0), vec3(0.5), max(gridX, gridY));\n"
"    gridColor = mix(gridColor, vec3(0.6), max(originX, originY));\n"
"\n"
"    FragColor = vec4(gridColor, 1.0);\n"
"}\n";
