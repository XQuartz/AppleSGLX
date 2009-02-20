/* 
 * These are special functions for stereoscopic support 
 * differences in MacOS X.
 */ 
void glDrawBuffer(GLenum mode) {
    GLenum buf[2];
    GLsizei n = 0;
	     
    if(GL_BACK == mode) {
	buf[0] = GL_BACK_LEFT;
	buf[1] = GL_BACK_RIGHT;
	n = 2;
    } else {
	buf[0] = mode;
	n = 1;
    }
    
    __gl_api.DrawBuffers(n, buf);
}

	 
void glDrawBuffers(GLsizei n, const GLenum *bufs) {
    GLenum newbuf[n + 2];
    GLsizei i, outi = 0;
    bool have_back = false;
    
    for(i = 0; i < n; ++i) {
	if(GL_BACK == bufs[i]) {
	    have_back = true;
	    continue;
	} else {
	    newbuf[outi++] = bufs[i];
	}
    }

    if(have_back) {
	newbuf[outi++] = GL_BACK_LEFT;
	newbuf[outi++] = GL_BACK_RIGHT;
    }
    
    __gl_api.DrawBuffers(outi, newbuf);
}

void glDrawBuffersARB(GLsizei n, const GLenum *bufs) {
    glDrawBuffers(n, bufs);
}
