#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

GLXPbuffer pbuf;

void dump_pbuffer(Display *dpy) {
	unsigned int width = 0, height = 0, fbid = 0;

	glXQueryDrawable(dpy, pbuf, GLX_WIDTH, &width);
	glXQueryDrawable(dpy, pbuf, GLX_HEIGHT, &height);
	glXQueryDrawable(dpy, pbuf, GLX_FBCONFIG_ID, &fbid);

	printf("queried drawable width %u height %u fbconfigID %x\n",
	       width, height, fbid);
}

void draw(Display *dpy) {
    GLenum err;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();   
    glColor3f(0.5f, 0.5f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f( 0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f,-1.0f, 0.0f);
    glVertex3f( 1.0f,-1.0f, 0.0f);
    glEnd();    
    glFinish();

    puts("RENDER");

    err = glGetError();
    if(GL_NO_ERROR != err) {
	fprintf(stderr, "an unexpect error occurred: %d\n", err);
	abort();
    }

    dump_pbuffer(dpy);
}

int main() {
    Display *dpy;
    int attrib[] = { 
	GLX_RED_SIZE, 8,
	GLX_GREEN_SIZE, 8,
	GLX_BLUE_SIZE, 8,
	GLX_DEPTH_SIZE, 24,
	GLX_RENDER_TYPE, GLX_RGBA_BIT,
	GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT | GLX_WINDOW_BIT,
	GLX_X_RENDERABLE, True,
	None,
    };
    int eventbase, errorbase;
    int screen;
    Window root, win;
    XVisualInfo *visinfo;
    XSetWindowAttributes attr;
    GLXContext ctx;
    GLXFBConfig *fbconfig;
    int numfbconfig;
    int pbattrib[] =  {
	GLX_PBUFFER_WIDTH, 400,
	GLX_PBUFFER_HEIGHT, 400,
	None
    };

    dpy = XOpenDisplay(NULL);
    
    if(NULL == dpy) {
        fprintf(stderr, "error: unable to open display!\n");
        return EXIT_FAILURE;
    }
    
    if(!glXQueryExtension(dpy, &eventbase, &errorbase)) {
        fprintf(stderr, "GLX is not available!\n");
        return EXIT_FAILURE;
    }
    
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    fbconfig = glXChooseFBConfig(dpy, screen, attrib, &numfbconfig);

    if(NULL == fbconfig) {
	fprintf(stderr, "error: couldn't choose a GLXFBConfig!\n");
	return EXIT_FAILURE;
    }

    visinfo = glXGetVisualFromFBConfig(dpy, fbconfig[0]);

    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask;

    win = XCreateWindow(dpy, root, /*x*/ 0, /*y*/ 0, 
			/*width*/ 400, /*height*/ 400,
			0, visinfo->depth, InputOutput,
			visinfo->visual, 
			CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			&attr);
   
    ctx = glXCreateContext(dpy, visinfo, NULL, True );

    if(!ctx) {
	fprintf(stderr, "error: glXCreateContext failed!\n");
	return EXIT_FAILURE;
    }
    
    XMapWindow(dpy, win);
   
    /*THIS ISN'T RIGHT YET.  We need to check the fbconfig for the Pbuffer bit. */
    pbuf = glXCreatePbuffer(dpy, fbconfig[0], pbattrib);
  
    printf("numfbconfig %d\n", numfbconfig);

    if(None == pbuf) {
	fprintf(stderr, "unable to create a GLXPbuffer!\n");
	return EXIT_FAILURE;
    }

    printf("pbuf %lx\n", pbuf);
    
    if(!glXMakeCurrent(dpy, pbuf, ctx)) {
	fprintf(stderr, "glXMakeCurrent failed!\n");
	return EXIT_FAILURE;
    }

    draw(dpy);
    glXDestroyPbuffer(dpy, pbuf);
    draw(dpy);
    /* This should release the final reference to the pbuffer. */
    glXMakeCurrent(dpy, None, ctx);
    printf("Expect a GLXBadDrawable error.\n");
    draw(dpy);
	
    return EXIT_SUCCESS;
}
