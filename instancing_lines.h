#ifndef INSTANCING_LINES_H
#define INSTANCING_LINES_H

void* instancing_lines_init_device( void );
uint32_t instancing_lines_update( void* device, const void* data, int32_t n_elems, int32_t elem_size, 
                                  float* mvp, float* viewport_size );
void instancing_lines_render( const void* device, const int32_t count );
void instancing_lines_term_device( void** );

#endif /* INSTANCING_LINES_H */

#ifdef INSTANCING_LINES_IMPLEMENTATION

typedef struct instancing_lines_device
{
  GLuint program_id;
  GLuint uniform_mvp_location;
  GLuint uniform_viewport_size_location;
  GLuint uniform_aa_radius_location;
  GLuint attrib_quad_pos_location;
  GLuint attrib_pos_width_0_location;
  GLuint attrib_col_0_location;
  GLuint attrib_pos_width_1_location;
  GLuint attrib_col_1_location;
  GLuint vao;
  GLuint line_vbo;
  GLuint quad_vbo;
  GLuint quad_ebo;
  float* mvp;
  float* viewport_size;
  float* aa_radius;
} instancing_lines_device_t;

void
instancing_lines_create_shader_program( instancing_lines_device_t* device )
{
  const char* vs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE(
      layout(location = 0) in vec3 quad_pos;
      layout(location = 1) in vec4 line_pos_width_0;
      layout(location = 2) in vec4 line_col_0;
      layout(location = 3) in vec4 line_pos_width_1;
      layout(location = 4) in vec4 line_col_1;
      
      layout(location = 0) uniform mat4 u_mvp;
      layout(location = 1) uniform vec2 u_viewport_size;
      layout(location = 2) uniform vec2 u_aa_radius;

      out vec4 v_col;
      out noperspective float v_u;
      out noperspective float v_v;
      out noperspective float v_line_width;
      out noperspective float v_line_length;

      void main()
      {
        float u_width = u_viewport_size[0];
        float u_height = u_viewport_size[1];
        float u_aspect_ratio = u_height / u_width;

        vec4 colors[2] = vec4[2]( line_col_0, line_col_1 );
        v_col = colors[ int(quad_pos.x) ];

        vec4 clip_pos_0 = u_mvp * vec4( line_pos_width_0.xyz, 1.0f );
        float line_width_0 = line_pos_width_0.w + u_aa_radius.x;
        
        vec4 clip_pos_1 = u_mvp * vec4( line_pos_width_1.xyz, 1.0f );
        float line_width_1 = line_pos_width_1.w + u_aa_radius.x;

        vec2 ndc_pos_0 = clip_pos_0.xy / clip_pos_0.w;
        vec2 ndc_pos_1 = clip_pos_1.xy / clip_pos_1.w;

        float extension_length = (1.5f + u_aa_radius.y);
        vec2 line = ndc_pos_1 - ndc_pos_0;
        vec2 dir = normalize( vec2( line.x, line.y * u_aspect_ratio ) );
        vec2 normal_0 = vec2( line_width_0/u_width, line_width_0/u_height ) * vec2(-dir.y, dir.x );
        vec2 normal_1 = vec2( line_width_1/u_width, line_width_1/u_height ) * vec2(-dir.y, dir.x );
        vec2 extension = vec2( extension_length / u_width, extension_length / u_height ) * dir;
        
        v_v = (1.0 - quad_pos.x) * (-1.0) + quad_pos.x * 1.0;
        v_u = quad_pos.y;
        v_line_width = (1.0 - quad_pos.x) * line_width_0 + quad_pos.x * line_width_1;
        v_line_length = 0.5* (length( line * u_viewport_size ) + 2.0 * extension_length);

        vec2 zw_part = (1.0 - quad_pos.x) * clip_pos_0.zw + quad_pos.x * clip_pos_1.zw;
        vec2 dir_y = quad_pos.y * ((1.0 - quad_pos.x) * normal_0 + quad_pos.x * normal_1);
        vec2 dir_x = quad_pos.x * line + v_v * extension;

        gl_Position = vec4( (ndc_pos_0 + dir_x + dir_y) * zw_part.y, zw_part );
      }
    );
  
  const char* fs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE(

      layout(location = 2) uniform vec2 u_aa_radius;

      in vec4 v_col;
      in noperspective float v_u;
      in noperspective float v_v;
      in noperspective float v_line_width;
      in noperspective float v_line_length;

      out vec4 frag_color;
      
      void main()
      {
        float au = 1.0 - smoothstep( 1.0 - ((2.0*u_aa_radius[0]) / v_line_width),  1.0, abs(v_u) );
        float av = 1.0 - smoothstep( 1.0 - ((2.0*u_aa_radius[1]) / v_line_length), 1.0, abs(v_v) );
        frag_color = v_col;
        frag_color.a *= min(av, au);
      }
    );

  GLuint vertex_shader   = glCreateShader( GL_VERTEX_SHADER );
  GLuint fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );

  glShaderSource( vertex_shader, 1, &vs_src, 0 );
  glCompileShader( vertex_shader );
  gl_utils_assert_shader_compiled( vertex_shader, "VERTEX_SHADER" );

  glShaderSource( fragment_shader, 1, &fs_src, 0 );
  glCompileShader( fragment_shader );
  gl_utils_assert_shader_compiled( fragment_shader, "FRAGMENT_SHADER" );


  device->program_id = glCreateProgram();
  glAttachShader( device->program_id, vertex_shader );
  glAttachShader( device->program_id, fragment_shader );
  glLinkProgram( device->program_id );
  gl_utils_assert_program_linked( device->program_id );

  glDetachShader( device->program_id, vertex_shader );
  glDetachShader( device->program_id, fragment_shader );
  glDeleteShader( vertex_shader );
  glDeleteShader( fragment_shader );

  device->attrib_quad_pos_location = glGetAttribLocation( device->program_id, "quad_pos" );
  device->attrib_pos_width_0_location = glGetAttribLocation( device->program_id, "line_pos_width_0" );
  device->attrib_col_0_location = glGetAttribLocation( device->program_id, "line_col_0" );
  device->attrib_pos_width_1_location = glGetAttribLocation( device->program_id, "line_pos_width_1" );
  device->attrib_col_1_location = glGetAttribLocation( device->program_id, "line_col_1" );

  device->uniform_mvp_location = glGetUniformLocation( device->program_id, "u_mvp" );
  device->uniform_aa_radius_location = glGetUniformLocation( device->program_id, "u_aa_radius" );
  device->uniform_viewport_size_location = glGetUniformLocation( device->program_id, "u_viewport_size" );
}

void
instancing_lines_setup_geometry_storage( instancing_lines_device_t* device )
{
  GLuint  binding_idx = 0;
  glCreateVertexArrays( 1, &device->vao );
  glCreateBuffers( 1, &device->line_vbo );
  glNamedBufferStorage( device->line_vbo, MAX_VERTS * sizeof(vertex_t), NULL, GL_DYNAMIC_STORAGE_BIT );

  glVertexArrayVertexBuffer( device->vao, binding_idx, device->line_vbo, 0, 2 * sizeof(vertex_t) );
  glVertexArrayBindingDivisor( device->vao, binding_idx, 1 );

  glEnableVertexArrayAttrib( device->vao, device->attrib_pos_width_0_location );
  glEnableVertexArrayAttrib( device->vao, device->attrib_col_0_location );
  glEnableVertexArrayAttrib( device->vao, device->attrib_pos_width_1_location );
  glEnableVertexArrayAttrib( device->vao, device->attrib_col_1_location );

  glVertexArrayAttribFormat( device->vao, device->attrib_pos_width_0_location, 4, GL_FLOAT, GL_FALSE, offsetof(vertex_t, pos_width) );
  glVertexArrayAttribFormat( device->vao, device->attrib_col_0_location, 4, GL_FLOAT, GL_FALSE, offsetof(vertex_t, col) );
  glVertexArrayAttribFormat( device->vao, device->attrib_pos_width_1_location, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_t) + offsetof(vertex_t, pos_width) );
  glVertexArrayAttribFormat( device->vao, device->attrib_col_1_location, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_t) + offsetof(vertex_t, col) );

  glVertexArrayAttribBinding( device->vao, device->attrib_pos_width_0_location, binding_idx );
  glVertexArrayAttribBinding( device->vao, device->attrib_col_0_location, binding_idx );
  glVertexArrayAttribBinding( device->vao, device->attrib_pos_width_1_location, binding_idx );
  glVertexArrayAttribBinding( device->vao, device->attrib_col_1_location, binding_idx );


  binding_idx++;

  // Figure out this parametrization.
  float quad[] = {  0.0, -1.0, 0.0,
                    0.0, 1.0, 0.0,
                    1.0, 1.0, 0.0,
                    1.0, -1.0, 0.0  };
  uint16_t ind[] = { 0, 1, 2,  0, 2, 3 };
  

  glCreateBuffers( 1, &device->quad_vbo );
  glCreateBuffers( 1, &device->quad_ebo );

  glNamedBufferStorage( device->quad_vbo, sizeof(quad), quad, GL_DYNAMIC_STORAGE_BIT );
  glNamedBufferStorage( device->quad_ebo, sizeof(ind), ind, GL_DYNAMIC_STORAGE_BIT );

  glVertexArrayVertexBuffer( device->vao, binding_idx, device->quad_vbo, 0, 3*sizeof(float) );
  glVertexArrayElementBuffer( device->vao, device->quad_ebo );

  glEnableVertexArrayAttrib( device->vao, device->attrib_quad_pos_location );
  glVertexArrayAttribFormat( device->vao, device->attrib_quad_pos_location, 3, GL_FLOAT, GL_FALSE, 0 );
  glVertexArrayAttribBinding( device->vao, device->attrib_quad_pos_location, binding_idx );
}

void*
instancing_lines_init_device( void )
{
  instancing_lines_device_t* device = malloc( sizeof(instancing_lines_device_t) );
  memset( device, 0, sizeof(instancing_lines_device_t) );
  instancing_lines_create_shader_program( device );
  instancing_lines_setup_geometry_storage( device );
  return device;
}

void
instancing_lines_term_device( void** device_in )
{
  instancing_lines_device_t* device = *device_in;
  glDeleteProgram( device->program_id );
  glDeleteBuffers( 1, &device->line_vbo );
  glDeleteBuffers( 1, &device->quad_vbo );
  glDeleteBuffers( 1, &device->quad_ebo );
  glDeleteVertexArrays( 1, &device->vao );
  free(device);
  *device_in = NULL;
}

uint32_t
instancing_lines_update( void* device_in, const void* data, int32_t n_elems, int32_t elem_size,
                         float* mvp, float* viewport_size )
{
  instancing_lines_device_t* device = device_in;
  device->mvp = mvp;
  device->viewport_size = viewport_size;
  glNamedBufferSubData( device->line_vbo, 0, n_elems*elem_size, data );
  return n_elems;
}

void
instancing_lines_render( const void* device_in, const int32_t count )
{
  const instancing_lines_device_t* device = device_in;
  glUseProgram( device->program_id );
  glUniformMatrix4fv( device->uniform_mvp_location, 1, GL_FALSE, device->mvp );
  glUniform2fv( device->uniform_viewport_size_location, 1,  device->viewport_size );
  glUniform2f( device->uniform_aa_radius_location, 2.0, 2.0 );

  glBindVertexArray( device->vao );
  glDrawElementsInstanced( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL, count>>1 );

  glBindVertexArray( 0 );
  glUseProgram( 0 );
}

#endif /*INSTANCING_LINES_IMPLEMENTATION*/