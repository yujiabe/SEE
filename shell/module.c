/*
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
# include <string.h>
#endif

#include <dlfcn.h>

#include <see/see.h>
#include "module.h"

int
load_module(name)
	const char *name;
{
	void *handle;
	struct SEE_module *module;
	char symname[1024];

	handle = dlopen(name, DL_LAZY);
	if (!handle && !strchr(name, '/')) {
	    char path[1024];
	    snprintf(path, sizeof path, "%s/lib%s.so", PATH_PKGLIB, name);
	    handle = dlopen(path, DL_LAZY);
	}
	if (!handle) {
	    fprintf(stderr, "%s\n", dlerror());
	    return 0;
	}

	module = dlsym(handle, "_module");
	if (!module)
		module = dlsym(handle, "module");
	if (!module) {
	    const char *p;
	    char *q;

	    /* Turn names of the form "/path/foo.ext" into "foo_module" */
	    for (p = name; *p; p++) {}
	    while (p != name && p[-1] != '/') p--;
	    q = symname;
	    *q++ = '_';
	    for (; *p && *p != '.'; p++)
		*q++ = *p;
	    for (p = "_module"; *p; p++)
	    	*q++ = *p;
	    *q = '\0';
	    module = dlsym(handle, symname);
	    if (!module)
		module = dlsym(handle, symname + 1);
	    if (!module && memcmp(symname + 1, "lib", 3) == 0) {
	    	symname[3] = '_';
	        module = dlsym(handle, symname + 3);
		if (!module)
		    module = dlsym(handle, symname + 4);
	    }
	    	    
	}
	if (!module) {
		fprintf(stderr, "%s\n", dlerror());
		dlclose(handle);
		return 0;
	}
	if (module->magic != SEE_MODULE_MAGIC) {
		fprintf(stderr, "%s: bad module magic number %x != %x\n", 
			name, module->magic, SEE_MODULE_MAGIC);
		dlclose(handle);
		return 0;
	}

	if (SEE_module_add(module) != 0) {
		fprintf(stderr, "error while loading module %s\n", name);
		dlclose(handle);
		return 0;
	}
	return 1;
}
