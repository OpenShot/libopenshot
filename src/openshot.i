%module openshot

/* Ignore overloaded operator= warning */
#pragma SWIG nowarn=362

%{

#include "../include/Cache.h"
#include "../include/Coordinate.h"
#include "../include/Exceptions.h"
#include "../include/FileReaderBase.h"
#include "../include/FileWriterBase.h"
#include "../include/FFmpegReader.h"
#include "../include/FFmpegWriter.h"
#include "../include/Fraction.h"
#include "../include/Frame.h"
#include "../include/FrameMapper.h"
#include "../include/FrameRate.h"
#include "../include/Player.h"
#include "../include/Point.h"
#include "../include/KeyFrame.h"
%}

/* Use standard exception handling  */
%include std_except.i
%include "std_string.i"

// Grab a Python function object as a Python object.
%typemap(in) PyObject *pyfunc {
	if (!PyCallable_Check($input)) {
		PyErr_SetString(PyExc_TypeError, "Need a callable object!");
		return NULL;
	}
	$1 = $input;
}


%{

/* This function matches the prototype of the normal C callback
   function for our widget. However, we use the clientdata pointer
   for holding a reference to a Python callable object. */

	#include <iostream>

	static void PythonCallBack(int frame, int width, int height, const Magick::PixelPacket *Pixels, void *clientdata)
	{
		PyObject *func;
		double    dres = 0;
		int i = 0;

		// CREATE ROWS OF PIXEL ARRAYS (which represent an image)
		PyObject *rows = PyList_New(height);
		for(int row_id = 0; row_id < height; row_id++)
		{
			PyObject *cols = PyList_New(width);
			for(int col_id = 0; col_id < width; col_id++)
			{
				// CREATE RGB LIST
				PyObject *pixels = PyList_New(3);
				PyList_SetItem(pixels, 0, Py_BuildValue("i", Pixels[i].red));
				PyList_SetItem(pixels, 1, Py_BuildValue("i", Pixels[i].green));
				PyList_SetItem(pixels, 2, Py_BuildValue("i", Pixels[i].blue));

				// ADD RGB PIXELS TO COL
				PyList_SetItem(cols, col_id, pixels);
				
				// Increment pixel iterator
				i++;
			}

			PyList_SetItem(rows, row_id, cols);
		}

		PyObject *arglist = PyTuple_New(4);
		PyTuple_SetItem(arglist, 0, Py_BuildValue("i", frame));
		PyTuple_SetItem(arglist, 1, Py_BuildValue("i", width));
		PyTuple_SetItem(arglist, 2, Py_BuildValue("i", height));
		PyTuple_SetItem(arglist, 3, rows);

		func = (PyObject *) clientdata;			// Get Python function
		PyEval_CallObject(func, arglist);		// Call Python

		Py_DECREF(arglist);						// Trash arglist
		Py_DECREF(rows);						// Trash list
	}

%}

%include "../include/Cache.h"
%include "../include/Coordinate.h"
%include "../include/Exceptions.h"
%include "../include/FileReaderBase.h"
%include "../include/FileWriterBase.h"
%include "../include/FFmpegReader.h"
%include "../include/FFmpegWriter.h"
%include "../include/Fraction.h"
%include "../include/Frame.h"
%include "../include/FrameMapper.h"
%include "../include/FrameRate.h"
%include "../include/Player.h"
%include "../include/Point.h"
%include "../include/KeyFrame.h"





// Attach a new method to our plot widget for adding Python functions
%extend openshot::Player {
	// Set a Python function object as a callback function
	// Note : PyObject *pyfunc is remapped with a typempap

	void set_pymethod(PyObject *pyfunc) {
		self->SetFrameCallback(PythonCallBack, (void *) pyfunc);
		Py_INCREF(pyfunc);
	}
}





