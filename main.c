/* gcc `pkg-config --cflags x11 xi glew` -o testout xinput2.c `pkg-config --libs x11 xi glew` -lm -lX11 -lGL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/glx.h>

//function for setting the pressure/x/y position var in int format based on an event
static int get_position(XIDeviceEvent *event, double position[3])
{
    //extra data from event
    double *valuator = event->valuators.values;
    //select the second axis
    valuator+=2;
    
    int axis = 2;
    //check if the event has a second axis, pretty sure the first half is unneeded
    if (event->valuators.mask_len >= axis && XIMaskIsSet(event->valuators.mask, axis)){
        position[0] = *valuator;
        position[1] = event->event_x;
        position[2] = event->event_y;
        //return that there was an update
        return 1;
    }
    //no update
    return 0;

    //for debugging in the future, loops over all input data
    /*
    for (int i = 0; i < event->valuators.mask_len * 8; i++)
    {
        //printf("a\n");
        //XIMaskIsSet(event->valuators.mask, i);
        //printf("b\n");
        if (XIMaskIsSet(event->valuators.mask, i))
        {
            printf("  acceleration on valuator %d: %f\n", i, *valuator);
            printf("  pos on valuator %f: %f\n", event->event_x, event->event_y);
            valuator++;
        }
    }
    */


}

//get a parabola where a b and c are evenly spaced points horizontally from 0 to 2^32, then sample that point vertically at x
long parabola (long long a, long long b, long long c, long long x)
{
    return (b - a - ((2147483648 - x) * (a - 2 * b + c)) / 4294967296 ) * x / 2147483648 + a;
}

//inifnitely differintiable step function
double smoothStep (unsigned long x)
{
    //bring to range of 0-1
    double X=(double)x / 4294967296;
    //shouldnt be possible
    if (X<=0)
        return 0;
    //should never occur, tho is possible
    if (X>=1)
        return 1;
    //to avoid double computing, idk if this is faster or not, but imo its more legible
    double ePow = exp(-1/X);
    return ePow / (ePow + exp(-1 / (1 - X)));
}

int main (int argc, char **argv)
{
    Display *dpy;
    int xi_opcode, event, error;
    Window win;
    XEvent ev;
    XWindowAttributes wn_attr;
    GLint gl_attr[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi;
    Colormap cmap;
    XSetWindowAttributes swa;
    GLXContext glc;
    double position[3];
    unsigned intposition[3];
    int working = 0;
    //unsigned long test = 0;

    //init a display
    dpy = XOpenDisplay(NULL);

    //check if display exists
    if (!dpy) {
        fprintf(stderr, "Failed to open display.\n");
        return -1;
    }

    //check if X11 accesable
    if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
           printf("X Input extension not available.\n");
              return -1;
    }

    //get the monitor's viewfield??
    Window root = DefaultRootWindow(dpy);

    //get settings for monitor??
    vi = glXChooseVisual(dpy, 0, gl_attr);
    //make sure that worked
    if (!vi) {
        fprintf(stderr, "No appropriate visual found.\n");
        return -1;
    }

    //get color space
    cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

    //add the color space to the settings
    swa.colormap = cmap;
    //and an event mask for rendering
    swa.event_mask = ExposureMask;// | KeyPressMask;

    //create the actual window
    win = XCreateWindow(dpy, root, 0, 0, 600, 600, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    //different kind of event mask
    XIEventMask mask;
    XSelectInput(dpy, win, ExposureMask);
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = calloc(mask.mask_len, sizeof(char));
    XISetMask(mask.mask, XI_Motion);
    //XISetMask(mask.mask, XI_KeyPress);
    //XISetMask(mask.mask, XI_KeyRelease);
    XISelectEvents(dpy, win, &mask, 1);
    free(mask.mask);
    //attach the window and display
    XMapWindow(dpy, win);
    //name it
    XStoreName(dpy, win, "Oil Painting");

    //inititalize opengl
    glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    //select the window
    glXMakeCurrent(dpy, win, glc);
    //initalize glew
    glewInit();

    //enable depth-testing
    glEnable(GL_DEPTH_TEST);

    //two tris make a quad
    static const float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
    };

    //vars for initializing opengl stuff
    unsigned int VAO;
    unsigned int VBO;
    unsigned int vertexShader;
    int  success;
    char infoLog[512];
    unsigned int fragmentShader;
    unsigned int shaderProgram;
    
    //create the arrays responsible for rendering the quad
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    //create the buffer responsible for rendering the quad
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    //vertex shader just outputs the input
    const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("Shader vertex complilation faliled.\n");
        return -1;
        //std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    //fragment shader for getting texture
    const char *fragmentShaderSource = "#version 330 core\n"
    "in vec2 UV;\n"
    //"in vec4 position;\n"
    "out vec3 color;\n"
    "uniform sampler2D renderedTexture;\n"
    "uniform float time;\n"
    "void main(){\n"
    //"    color = texture( renderedTexture, UV).xyz;\n"
    "    color = vec3( UV, 0.0);\n"
    //"    color = vec3( position[0], position[1], position[2]);\n"
    //"    color = vec3( 0., 0.2, UV[0]+UV[1]);\n"
    "}\n";
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("Shader fragment complilation faliled.\n");
        return -1;
        //std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    //convert the seperate shaders into one program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("Shader linking faliled.\n");
        return -1;
    }

    //use it and clear up space
    glUseProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader); 

    //tell it basically "I'm using tris"??
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //enable program
    glUseProgram(shaderProgram);

    //set base color (black)
    glClearColor(0.0, 0.0, 0.0, 1.0);

    /*
    unsigned int FramebufferName = 0;
    glGenFramebuffers(1, &FramebufferName);
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
    unsigned int renderedTexture;
    glGenTextures(1, &renderedTexture);
    glBindTexture(GL_TEXTURE_2D, renderedTexture);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, 1024, 768, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    //unsigned int depthrenderbuffer;
    //glGenRenderbuffers(1, &depthrenderbuffer);
    //glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1024, 768);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderedTexture, 0);
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);

    //glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
    //glViewport(0,0,1024,768);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("frame buffer faliled.\n");
        return -1;
    }


            //glBindFramebuffer(GL_FRAMEBUFFER, 0);
            //glViewport(0,0,1024,768);
    */



    while(1)
    {
        XGenericEventCookie *cookie = &ev.xcookie;

        XNextEvent(dpy, &ev);

        if(ev.type == Expose) {
            
            XGetWindowAttributes(dpy, win, &wn_attr);
            glViewport(0, 0, wn_attr.width, wn_attr.height);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glXSwapBuffers(dpy, win);
            printf("exposed\n");
        }
        
        if (cookie->type != GenericEvent ||
            cookie->extension != xi_opcode ||
            !XGetEventData(dpy, cookie))
            continue;

        //printf("EVENT TYPE %d\n", cookie->evtype);
        switch(cookie->evtype)
        {
            case XI_Motion:
                //printf("non-doing\n");
                if (get_position(cookie->data, position)){
                    working = 1;
                    XGetWindowAttributes(dpy, win, &wn_attr);
                    //intposition[0]=position[0];
                    if (position[0]>2000)
                        intposition[0]=65535;
                    else
                        intposition[0]=position[0]*65535/2000;
                    if (intposition[0] == 0)
                        intposition[0]=1;
                    intposition[1]=position[1]*65535/wn_attr.width;
                    intposition[2]=position[2]*65535/wn_attr.height;
                    printf("  pos on valuator %u, %u, %u aka %f, %f, %f\n", intposition[0], intposition[1], intposition[2], position[0], position[1], position[2]);
                    //printf("  test %ld, %f, %lu, %f\n", parabola(intposition[0], intposition[1], intposition[2],test), smoothStep(test), test, (double)test/4294967295);
                    //test=test+123456;
                    
                    //printf("doing\n");
                }else{
                    if (working){
                        working = 0;
                        printf("break\n");
                        
                    }
                        
                }
                break;
            default:
                printf("EVENT TYPE %d\n", cookie->evtype);
                break;
        }

        XFreeEventData(dpy, cookie);
    }
    glXMakeCurrent(dpy, None, NULL);
 	glXDestroyContext(dpy, glc);
 	XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

