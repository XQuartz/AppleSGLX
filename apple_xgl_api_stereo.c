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
    GLenum newbuf[n + 1];
    GLsizei i, outi = 0;
    bool back_handled = false;
    
    for(i = 0; i < n; ++i) {
	if(!back_handled && GL_BACK == bufs[i]) {
	    newbuf[outi++] = GL_BACK_LEFT;
	    newbuf[outi++] = GL_BACK_RIGHT;
	    back_handled = true;
	} else {
	    newbuf[outi++] = bufs[i];
	}
    }
    
    __gl_api.DrawBuffers(outi, newbuf);
}

void glDrawBuffersARB(GLsizei n, const GLenum *bufs) {
    __gl_api.DrawBuffers(n, bufs);
}
