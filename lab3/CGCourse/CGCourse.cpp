#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp> 
#include <glm/ext/matrix_transform.hpp> 
#include <glm/ext/matrix_clip_space.hpp> 
#include <glm/gtc/type_ptr.hpp>
#include <SOIL.h>

using namespace std;

GLFWwindow* g_window;

GLuint g_shaderProgram;
GLint g_uMVP;
GLint g_uMV;
GLint g_uMV_TR_INV;

glm::mat4 Projection;
glm::mat4 Model;
glm::mat4 View;
glm::mat4 MV;

GLuint texture1;
GLuint texture2;
GLint mapLocation1;
GLint mapLocation2;

class Modell
{
public:
    GLuint vbo;
    GLuint ibo;
    GLuint vao;
    GLsizei indexCount;
};

Modell g_model;

GLuint createShader(const GLchar* code, GLenum type)
{
    GLuint result = glCreateShader(type);

    glShaderSource(result, 1, &code, NULL);
    glCompileShader(result);

    GLint compiled;
    glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            char* infoLog = (char*)alloca(infoLen);
            glGetShaderInfoLog(result, infoLen, NULL, infoLog);
            cout << "Shader compilation error" << endl << infoLog << endl;
        }
        glDeleteShader(result);
        return 0;
    }
    return result;
}

GLuint createProgram(GLuint vsh, GLuint fsh)
{
    GLuint result = glCreateProgram();

    glAttachShader(result, vsh);
    glAttachShader(result, fsh);

    glLinkProgram(result);

    GLint linked;
    glGetProgramiv(result, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            char* infoLog = (char*)alloca(infoLen);
            glGetProgramInfoLog(result, infoLen, NULL, infoLog);
            cout << "Shader program linking error" << endl << infoLog << endl;
        }
        glDeleteProgram(result);
        return 0;
    }

    return result;
}

bool createShaderProgram()
{
    g_shaderProgram = 0;

    const GLchar vsh[] =
        "#version 330\n"
        ""
        "layout(location = 0) in vec2 a_position;"
        ""
        "uniform mat4 u_mvp;"
        "uniform mat4 u_mv;"
        "uniform mat3 u_mv_tr_inv;"
        ""
        "out vec3 v_normal;"
        "out vec3 v_pos;"
        "out vec2 v_texCoord;"

        ""
        "float f(vec2 p) { return 1.5 * sin(p.x * p.y); }"
        "vec3 grad(vec2 p) {return vec3(-1.5 * p.y * cos(p.x * p.y), 1.0, -1.5 * p.x * cos(p.x * p.y)); };"


        //"float f(vec2 p) { return sin(p.x) * cos(p.y); }"
        //"vec3 grad(vec2 p) { return vec3(-cos(p.x) * cos(p.y), 1.0, sin(p.x) * sin(p.y)); };"

        ""
        "void main()"
        "{"
        "   float y = f(a_position);"
        "   vec4 p0 = vec4(a_position[0], y, a_position[1], 1.0);"
        "   v_normal = transpose(inverse(mat3(u_mv))) * normalize(grad(a_position));"
        //"   v_normal = u_mv_tr_inv * normalize(grad(a_position));"
        "   v_pos = vec3(u_mv * p0);"
        "   gl_Position = u_mvp * p0;"
        "   v_texCoord = a_position / 4 + 0.5;"
        "}"
        ;

    const GLchar fsh[] =
        "#version 330\n"
        ""
        "uniform sampler2D u_map1;"
        "uniform sampler2D u_map2;"
        ""
        "in vec3 v_normal;"
        "in vec3 v_pos;"
        "in vec2 v_texCoord;"
        ""
        "layout(location = 0) out vec4 o_color;"
        ""
        "void main()"
        "{"
        "   float S = 10.0;"
        "   vec4 tex1 = texture(u_map1, v_texCoord);"
        "   vec4 tex2 = texture(u_map2, v_texCoord);"
        "   vec3 color = vec3(mix(tex1, tex2, 0.7));"
        "   vec3 n = normalize(v_normal);"
        "   vec3 E = vec3(0.0, 0.0, 0.0);"
        "   vec3 L = vec3(5.0 , 3.0, 0.0);"
        "   vec3 l = normalize(v_pos - L);"
        "   float d = max(dot(n, -l), 0.3);"
        "   vec3 e = normalize(E - v_pos);"
        "   vec3 h = normalize(-l + e);"
        "   float s = pow(max(dot(n, h), 0.0), S);"
        "   o_color = vec4(color * d + s * vec3(1, 1, 1), 1);"
        //"   o_color = vec4(color, 1);"
        //"   o_color = tex2;"
        "}"
        ;

    GLuint vertexShader, fragmentShader;

    vertexShader = createShader(vsh, GL_VERTEX_SHADER);
    fragmentShader = createShader(fsh, GL_FRAGMENT_SHADER);

    g_shaderProgram = createProgram(vertexShader, fragmentShader);

    g_uMVP = glGetUniformLocation(g_shaderProgram, "u_mvp");
    g_uMV = glGetUniformLocation(g_shaderProgram, "u_mv");
    g_uMV_TR_INV = glGetUniformLocation(g_shaderProgram, "u_mv_tr_inv");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return g_shaderProgram != 0;
}

bool createModel()
{
    const int N = 100;
    GLfloat* vertices = new GLfloat[2 * (N + 1) * (N + 1)];
    GLuint* indices = new GLuint[6 * N * N];
    
    int ind_vertex = 0;
   
    for (size_t i = 0; i < N + 1; i++) {
        for (size_t j = 0; j < N + 1; j++) {
            auto vertex = &vertices[(i * (N + 1) + j) * 2];
            vertex[0] = GLfloat(i) / (25.0) - 2.04f;
            vertex[1] = GLfloat(j) / (25.0) - 2.04f;
        }
    }

    int ind = 0;
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < N; j++) {
            auto idx = &indices[(i * N + j) * 6];
            idx[0] = i * (N + 1) + j;
            idx[1] = i * (N + 1) + (j + 1);
            idx[2] = (i + 1) * (N + 1) + (j + 1);
           
            idx[3] = (i + 1) * (N + 1) + (j + 1);
            idx[4] = (i + 1) * (N + 1) + j;
            idx[5] = i * (N + 1) + j;
        }
    }

    glGenVertexArrays(1, &g_model.vao);
    glBindVertexArray(g_model.vao);

    glGenBuffers(1, &g_model.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_model.vbo);
    glBufferData(GL_ARRAY_BUFFER, (N+1) * (N+1) * 2 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &g_model.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_model.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * N * N * sizeof(GLuint), indices, GL_STATIC_DRAW);

    g_model.indexCount = 6 * N * N;

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const GLvoid*)0);

    delete[] vertices;
    delete[] indices;

    return g_model.vbo != 0 && g_model.ibo != 0 && g_model.vao != 0;
}

bool loadTexture()
{
    int texW, texH;
    // 1
    
    
    unsigned char* image = SOIL_load_image("D:/Desktop/ПМИ/3_курс/Вычислительная геометрия и алгоритмы компьютерной графики/cgCourse/lab3/CGCourse/textures/texture1.jpg", &texW, &texH, 0, SOIL_LOAD_RGB);
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    SOIL_free_image_data(image);
    // настройка текстурных параметров, режим повторений
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // настройка минификации/магнификации
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //билинейная фильтрация
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    mapLocation1 = glGetUniformLocation(g_shaderProgram, "u_map1");
    
    // 2
    
    image = SOIL_load_image("D:/Desktop/ПМИ/3_курс/Вычислительная геометрия и алгоритмы компьютерной графики/cgCourse/lab3/CGCourse/textures/texture2.jpg", &texW, &texH, 0, SOIL_LOAD_RGB);
    glGenTextures(1, &texture2);
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    SOIL_free_image_data(image);
    // настройка текстурных параметров, режим повторений
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // настройка минификации/магнификации
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //билинейная фильтрация
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    mapLocation2 = glGetUniformLocation(g_shaderProgram, "u_map2");
    return true;
}

bool init()
{
    // Set initial color of color buffer to white.
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);

   

    return createShaderProgram() && createModel() && loadTexture();;
}

void reshape(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


void draw()
{
    // Clear color buffer.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_shaderProgram);
    glBindVertexArray(g_model.vao);
    
    // вращение камеры вокруг модели
    /*
    GLfloat radius = 10.0f;
    GLfloat camX = sin(glfwGetTime()) * radius;
    GLfloat camZ = cos(glfwGetTime()) * radius;
   
    View = glm::lookAt(glm::vec3(camX, 0.0, camZ), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
    MV = View * Model;
    */
    /////////////////////////// 

    // вращение модели вокруг оси y
    MV = glm::rotate(MV, glm::radians(1.0f / 25.0f), glm::vec3(0.0, 1.0, 0.0));

    glm::mat3 MV_TR_INV = glm::transpose(glm::inverse(glm::mat3(MV)));

    glUniformMatrix4fv(g_uMVP, 1, GL_FALSE, glm::value_ptr(Projection * MV));
    glUniformMatrix4fv(g_uMV, 1, GL_FALSE, glm::value_ptr(MV));
    glUniformMatrix4fv(g_uMV_TR_INV, 1, GL_FALSE, glm::value_ptr(MV_TR_INV));

    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glUniform1i(mapLocation1, 0);
    

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture2);
    glUniform1i(mapLocation2, 1);


    glDrawElements(GL_TRIANGLES, g_model.indexCount, GL_UNSIGNED_INT, NULL);
}

void cleanup()
{
    if (g_shaderProgram != 0)
        glDeleteProgram(g_shaderProgram);
    if (g_model.vbo != 0)
        glDeleteBuffers(1, &g_model.vbo);
    if (g_model.ibo != 0)
        glDeleteBuffers(1, &g_model.ibo);
    if (g_model.vao != 0)
        glDeleteVertexArrays(1, &g_model.vao);

    glDeleteTextures(1, &texture1);
    glDeleteTextures(1, &texture2);
}

bool initOpenGL()
{
    // Initialize GLFW functions.
    if (!glfwInit())
    {
        cout << "Failed to initialize GLFW" << endl;
        return false;
    }

    // Request OpenGL 3.3 without obsoleted functions.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window.
    g_window = glfwCreateWindow(800, 600, "OpenGL Test", NULL, NULL);
    if (g_window == NULL)
    {
        cout << "Failed to open GLFW window" << endl;
        glfwTerminate();
        return false;
    }

    // Initialize OpenGL context with.
    glfwMakeContextCurrent(g_window);

    // Set internal GLEW variable to activate OpenGL core profile.
    glewExperimental = true;

    // Initialize GLEW functions.
    if (glewInit() != GLEW_OK)
    {
        cout << "Failed to initialize GLEW" << endl;
        return false;
    }

    // Ensure we can capture the escape key being pressed.
    glfwSetInputMode(g_window, GLFW_STICKY_KEYS, GL_TRUE);

    // Set callback for framebuffer resizing event.
    glfwSetFramebufferSizeCallback(g_window, reshape);

    return true;
}

void tearDownOpenGL()
{
    // Terminate GLFW.
    glfwTerminate();
}

int main()
{
    // Initialize OpenGL
    if (!initOpenGL())
        return -1;

    // Initialize graphical resources.
    bool isOk = init();

    Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.f);

    Model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f));

    View = glm::lookAt(glm::vec3(-6.0, 5.0, 0.0), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0, 1.0, 0.0));

    MV = View * Model;

    if (isOk)
    {
        // Main loop until window closed or escape pressed.
        while (glfwGetKey(g_window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(g_window) == 0)
        {
            // Draw scene.
            draw();

            // Swap buffers.
            glfwSwapBuffers(g_window);
            // Poll window events.
            glfwPollEvents();
        }
    }

    // Cleanup graphical resources.
    cleanup();

    // Tear down OpenGL.
    tearDownOpenGL();
    return isOk ? 0 : -1;

    
}