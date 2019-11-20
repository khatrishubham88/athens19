#include <stdio.h>
#include <stdlib.h>
#include <iostream> // for standard I/O
#include <fstream>
#include <time.h>
#include "opencv2/opencv.hpp"


#include <CL/cl.h>
#include <CL/cl_ext.h>

using namespace cv;
using namespace std;

#define SHOW
// 1 ... gauß, 2 ... sobel, else ... both
#define FILTER 1

void multiplyKernels(char* kernel1, char* kernel2, char* output) {
	for(char i = 0; i < 3; ++i) {
        for(char j = 0; j < 3; ++j) {
            int currentRow = 3*i;
            output[currentRow + j] = 0;
            char sum = 0;
            for(char k = 0; k<3; ++k) {
                sum += kernel1[currentRow + k]*kernel2[3*k + j];
            }
            output[currentRow + j] = sum;
        }
    }
}

void copyArray(char* input, int len,  char* output) {
	for(int i = 0; i <len; ++i) {
		output[i] = input [i];
	}
}

// TODO introduce some error handling
// Code adapted from https://gist.github.com/courtneyfaulkner/7919509
cl_int findPlatforms(cl_platform_id* platforms, cl_uint* platformCountPointer) {
    cl_uint i, j;
    char* info;
    size_t infoSize;

    cl_uint platformCount = *platformCountPointer;

    //Extensions are commented out (change array sizes to 5)
    const char* attributeNames[4] = { "Name", "Vendor",
        "Version", "Profile"}; //, "Extensions" };
    const cl_platform_info attributeTypes[4] = { CL_PLATFORM_NAME, CL_PLATFORM_VENDOR,
        CL_PLATFORM_VERSION, CL_PLATFORM_PROFILE};
        //, CL_PLATFORM_EXTENSIONS };
    const int attributeCount = sizeof(attributeNames) / sizeof(char*);

    //get platform count
    clGetPlatformIDs(5, NULL, &platformCount);

    // get all platforms
    platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * platformCount);
    clGetPlatformIDs(platformCount, platforms, NULL);

    // for each platform print all attributes
    for (i = 0; i < platformCount; i++) {
        printf("\n %d. Platform \n", i+1);
        for (j = 0; j < attributeCount; j++) {

            // get platform attribute value size
            clGetPlatformInfo(platforms[i], attributeTypes[j], 0, NULL, &infoSize);
            info = (char*) malloc(infoSize);
            
            // get platform attribute value
            clGetPlatformInfo(platforms[i], attributeTypes[j], infoSize, info, NULL);

            printf("  %d.%d %-11s: %s\n", i+1, j+1, attributeNames[j], info);
            free(info);
        }
        printf("\n");
    }
    return 0;
}

// Code adapted from https://gist.github.com/courtneyfaulkner/7919509
cl_device_id obtainDevice(cl_platform_id* platforms, cl_uint* platformCountPointer, int deviceIndex) {
    cl_uint i, j;
    char* value;
    size_t valueSize;
    cl_uint maxComputeUnits;

    cl_uint deviceCount;
    cl_device_id* devices = NULL;
    cl_device_id deviceToUse;

    cl_uint platformCount = *platformCountPointer;

    for (i = 0; i < platformCount; i++) {
        cl_device_type devices_to_search_for = CL_DEVICE_TYPE_GPU; //ALL

        // get all devices
        clGetDeviceIDs(platforms[i], devices_to_search_for, 0, NULL, &deviceCount);
        devices = (cl_device_id*) malloc(sizeof(cl_device_id) * deviceCount);
        clGetDeviceIDs(platforms[i], devices_to_search_for, deviceCount, devices, NULL);

        // for each device print critical attributes
        for (j = 0; j < deviceCount; j++) {

            // print device name
            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, 0, NULL, &valueSize);
            value = (char*) malloc(valueSize);
            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, valueSize, value, NULL);
            printf("%d. Device: %s\n", j+1, value);
            free(value);

            // print hardware device version
            clGetDeviceInfo(devices[j], CL_DEVICE_VERSION, 0, NULL, &valueSize);
            value = (char*) malloc(valueSize);
            clGetDeviceInfo(devices[j], CL_DEVICE_VERSION, valueSize, value, NULL);
            printf(" %d.%d Hardware version: %s\n", j+1, 1, value);
            free(value);

            // print software driver version
            clGetDeviceInfo(devices[j], CL_DRIVER_VERSION, 0, NULL, &valueSize);
            value = (char*) malloc(valueSize);
            clGetDeviceInfo(devices[j], CL_DRIVER_VERSION, valueSize, value, NULL);
            printf(" %d.%d Software version: %s\n", j+1, 2, value);
            free(value);

            // print c version supported by compiler for device
            clGetDeviceInfo(devices[j], CL_DEVICE_OPENCL_C_VERSION, 0, NULL, &valueSize);
            value = (char*) malloc(valueSize);
            clGetDeviceInfo(devices[j], CL_DEVICE_OPENCL_C_VERSION, valueSize, value, NULL);
            printf(" %d.%d OpenCL C version: %s\n", j+1, 3, value);
            free(value);

            // print parallel compute units
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS,
                    sizeof(maxComputeUnits), &maxComputeUnits, NULL);
            printf(" %d.%d Parallel compute units: %d\n", j+1, 4, maxComputeUnits);
        }

        // Device specific code here

        // Assumtion from here on: use just one device
        
    }
        deviceToUse = devices[deviceIndex];
        free(devices);
        return deviceToUse;
}

void callback(const char *buffer, size_t length, size_t final, void *user_data) {
     fwrite(buffer, 1, length, stdout);
}

void print_clbuild_errors(cl_program program,cl_device_id device) {
    cout<<"Program Build failed\n";
    size_t length;
    char buffer[2048];
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &length);
    cout<<"--- Build log ---\n "<<buffer<<endl;
    exit(1);
}

char ** read_file(const char *name) {
    size_t size;
    char **output = (char **)malloc(sizeof(char *));
    FILE* fp = fopen(name, "rb");
    if (!fp) {
        printf("no such file:%s",name);
        exit(-1);
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *output = (char *)malloc(size);
    char **outputstr=(char **)malloc(sizeof(char *));
    *outputstr= (char *)malloc(size);
    if (!*output) {
        fclose(fp);
        printf("mem allocate failure:%s",name);
        exit(-1);
    }

    if(!fread(*output, size, 1, fp)) printf("failed to read file\n");
    fclose(fp);

    snprintf((char *)*outputstr,size,"%s\n",*output);
    return outputstr;
}

void checkError(int status, const char *msg) {
    if(status!=CL_SUCCESS) {
        printf("%s\n",msg);
    }
}

int main(int, char**)
{
	const int RESX = 640;
	const int RESY = 360;

	char* inputFrame = (char*) malloc(RESY*RESX*sizeof(char));
	char* filteredFrame = (char*) malloc(RESY*RESX*sizeof(char));

	// Setup of filters
	char filterKernel[9] = {};
	char *filterFactor;

	char kernelGauss[9] = {1,2,1,
						2,4,2,
						1,2,1};
	char factorGauss = 16;

	/*char kernelSobel[9] = {-1,-1,-1,
						-1, 8,-1,
						-1, 1,-1};
	char factorSobel = 1;*/

	if(FILTER == 1) {
		copyArray(kernelGauss, 3, filterKernel);
		*filterFactor = factorGauss;
	} /*else if (FILTER == 2) {
		copyArray(kernelSobel, 3, filterKernel);
		*filterFactor = factorSobel;
	} else {
		multiplyKernels(kernelGauss, kernelSobel, filterKernel);
		*filterFactor = factorGauss*factorSobel;
	}*/

	// OpenCL setup
	cl_uint platformCount;
	cl_platform_id* platforms;
	cl_platform_id platformToUse;
	cl_device_id device;
	cl_context context;
	cl_context_properties context_properties[] =
	{
		CL_CONTEXT_PLATFORM, 0,
		CL_PRINTF_CALLBACK_ARM, (cl_context_properties)callback,
		CL_PRINTF_BUFFERSIZE_ARM, 0x1000,
		0
	};
	cl_command_queue queue;
	cl_program program;
	cl_kernel kernel;
	cl_int errorcode;
	int status;

    cl_event kernel_event;

	cl_mem input_buf, filter_buf, factor_buf, output_buf;

	platforms = NULL;
	findPlatforms(platforms, &platformCount);

	platformToUse = platforms[0];

	device = obtainDevice(platforms, &platformCount, 0);

	context_properties[1] = (cl_context_properties)platformToUse;
	context = clCreateContext(context_properties, 1, &device, NULL, NULL, NULL);
	queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, NULL);
	char **opencl_program=read_file("videofilter.cl");
    program = clCreateProgramWithSource(context, 1, (const char **)opencl_program, NULL, NULL);
     if (program == NULL)
      {
             printf("Program creation failed\n");
             return 1;
      }
     int success=clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
     if(success!=CL_SUCCESS) print_clbuild_errors(program,device);
     kernel = clCreateKernel(program, "filter", NULL);

	// Input buffers
    input_buf = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR,
       RESX*RESY*sizeof(char), NULL, &status);
    checkError(status, "Failed to create buffer for input A");

    filter_buf = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR,
        9*sizeof(char), NULL, &status);
    checkError(status, "Failed to create buffer for input B");

    factor_buf = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR,
    	sizeof(char), NULL, &status);
    checkError(status, "Failed to create buffer for output");

	// Output buffer.
    output_buf = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR,
        RESX*RESY*sizeof(char), NULL, &status);
    checkError(status, "Failed to create buffer for output");

	// Kernel setup

	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_buf);
    checkError(status, "Failed to set argument 0");

    status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &filter_buf);
    checkError(status, "Failed to set argument 1");

    status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &factor_buf);
    checkError(status, "Failed to set argument 2");

    status = clSetKernelArg(kernel, 3, sizeof(cl_mem), &output_buf);
    checkError(status, "Failed to set argument 3");

    // Input mapping
	char * input = (char *)clEnqueueMapBuffer(queue, input_buf, CL_TRUE,
        CL_MAP_WRITE,0, RESX*RESY*sizeof(char), 0, NULL, NULL, &errorcode);
    checkError(errorcode, "Failed to map input");
    char * filter = (char *)clEnqueueMapBuffer(queue, filter_buf, CL_TRUE,
        CL_MAP_WRITE,0, 9*sizeof(char), 0, NULL, NULL, &errorcode);
    checkError(errorcode, "Failed to map filter");
	char * factor = (char *)clEnqueueMapBuffer(queue, factor_buf, CL_TRUE,
        CL_MAP_WRITE,0, sizeof(char), 0, NULL, NULL, &errorcode);
    checkError(errorcode, "Failed to map factor");
	char * output = (char *)clEnqueueMapBuffer(queue, output_buf, CL_TRUE,
        CL_MAP_WRITE,0, RESX*RESY*sizeof(char), 0, NULL, NULL, &errorcode);
    checkError(errorcode, "Failed to map output");

	


	// Video setup
    VideoCapture camera("./bourne.mp4");
    if(!camera.isOpened())  // check if we succeeded
        return -1;

    const string NAME = "./output.avi";   // Form the new name with container
    int ex = static_cast<int>(CV_FOURCC('M','J','P','G'));
    Size S = Size((int) camera.get(CV_CAP_PROP_FRAME_WIDTH),    // Acquire input size
                  (int) camera.get(CV_CAP_PROP_FRAME_HEIGHT));
	//Size S =Size(1280,720);
	cout << "SIZE:" << S << endl;
	
    VideoWriter outputVideo;                                        // Open the output
        outputVideo.open(NAME, ex, 25, S, true);

    if (!outputVideo.isOpened())
    {
        cout  << "Could not open the output video for write: " << NAME << endl;
        return -1;
    }
	time_t start,end;
	double diff,tot;
	int count=0;
	const char *windowName = "filter";   // Name shown in the GUI window.
    #ifdef SHOW
    namedWindow(windowName); // Resizable window, might not work on Windows.
    #endif

	// Framewise computation

    while (true) {
        Mat cameraFrame,displayframe;
		count=count+1;
		if(count > 299) break;
        camera >> cameraFrame;
        Mat filterframe = Mat(cameraFrame.size(), CV_8UC3);
        Mat grayframe, edge_x,edge_y,edge,edge_inv;

    	cvtColor(cameraFrame, grayframe, CV_BGR2GRAY);

		//TODO hinig, geht sicher irgendwie
		//copyMakeBorder(grayframe, greyframePadded, 1, 1, 1, 1, BORDER_REPLICATE);

		// Mat to array
		
		//TODO modularity
		memcpy(inputFrame, (char*)grayframe.data, displayframe.step * displayframe.rows * sizeof(char));

		/*
		time (&start);
    	GaussianBlur(grayframe, grayframe, Size(3,3),0,0);
    	GaussianBlur(grayframe, grayframe, Size(3,3),0,0);
    	GaussianBlur(grayframe, grayframe, Size(3,3),0,0);
		Scharr(grayframe, edge_x, CV_8U, 0, 1, 1, 0, BORDER_DEFAULT );
		Scharr(grayframe, edge_y, CV_8U, 1, 0, 1, 0, BORDER_DEFAULT );
		addWeighted( edge_x, 0.5, edge_y, 0.5, 0, edge );
        threshold(edge, edge, 80, 255, THRESH_BINARY_INV);
		time (&end);
		*/
	
        //cvtColor(edge, edge_inv, CV_GRAY2BGR);


    	// Clear the output image to black, so that the cartoon line drawings will be black (ie: not drawn).
    	
		//TODO convert array to frames
		memcpy((char*)grayframe.data, filteredFrame, displayframe.step * displayframe.rows*sizeof(char));

		memset((char*)displayframe.data, 0, displayframe.step * displayframe.rows);
		grayframe.copyTo(displayframe);
		outputVideo << displayframe;
	#ifdef SHOW
        imshow(windowName, displayframe);
	#endif
		diff = difftime (end,start);
		tot+=diff;
	}
	outputVideo.release();
	camera.release();
  	printf ("FPS %.2lf .\n", 299.0/tot );


	free(inputFrame);
	free(filteredFrame);
	free(platforms);


    return EXIT_SUCCESS;

}
