#include "FiveCell.hpp"

#include <cstdio>
#include <cstdarg>
#include <random>
#include <ctime>
#include <assert.h>
#include <math.h>
#include <cmath>
#include <iostream>

#include "stb_image.h"

#ifdef __APPLE__ 
#include "GLFW/glfw3.h"
#elif _WIN32 
#include "glfw3.h"
#endif

#include "SystemInfo.hpp"
#include "ShaderManager.hpp"

#define PI 3.14159265359

#ifndef _countof
#define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

bool FiveCell::setup(std::string csd){

//************************************************************
//Csound performance thread
//************************************************************
	std::string csdName = "";
	if(!csd.empty()) csdName = csd;
	session = new CsoundSession(csdName);

#ifdef _WIN32
	session->SetOption("-b -32"); 
	session->SetOption("-B 2048");
#endif

	session->StartThread();
	session->PlayScore();

	std::string val1 = "azimuth";
	const char* azimuth = val1.c_str();	
	if(session->GetChannelPtr(hrtfVals[0], azimuth, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
		std::cout << "GetChannelPtr could not get the azimuth input" << std::endl;
		return false;
	}
	std::string val2 = "elevation";
	const char* elevation = val2.c_str();
	if(session->GetChannelPtr(hrtfVals[1], elevation, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
		std::cout << "GetChannelPtr could not get the elevation input" << std::endl;
		return false;
	}	
	std::string val3 = "distance";
	const char* distance = val3.c_str();
	if(session->GetChannelPtr(hrtfVals[2], distance, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
		std::cout << "GetChannelPtr could not get the distance input" << std::endl;
		return false;
	}
	const char* randFreq = "randFreq";
	if(session->GetChannelPtr(randomFrequencyVal, randFreq, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
		std::cout << "GetChannelPtr could not get the randFreq value" << std::endl;
		return false;
	} 

	const char* sineVal = "sineControlVal";
	if(session->GetChannelPtr(m_cspSineControlVal, sineVal, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
		std::cout << "GetChannelPtr could not get the sineControlVal value" << std::endl;
		return false;
	}

	
	for(int i = 0; i < MAX_MANDEL_STEPS; i++){

		std::string mandelEscapeValString = "mandelEscapeVal" + std::to_string(i);
		const char* mandelEscapeVal = mandelEscapeValString.c_str();
		if(session->GetChannelPtr(m_cspMandelEscapeVals[i], mandelEscapeVal, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
			std::cout << "GetChannelPtr could not get the mandelEscapeVal " << std::to_string(i) << " value" << std::endl;
			return false;
		}

	}
	
	const char* mandelMaxPoints = "mandelMaxPoints";
	if(session->GetChannelPtr(m_cspMaxSteps, mandelMaxPoints, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
		std::cout << "GetChannelPtr could not get the mandelMaxPoints value" << std::endl;
		return false;
	}

//********* output values from csound to avr *******************//

	const char* rmsOut = "rmsOut";
	if(session->GetChannelPtr(m_pRmsOut, rmsOut, CSOUND_OUTPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0){
		std::cout << "Csound output value rmsOut not available" << std::endl;
		return false;
	}

//**********************************************************


//**********************************************************
// Lighting Components
//**********************************************************

	m_vec3MoonDirection = glm::vec3(-0.2f, -1.0f, -0.3f);
	m_vec3MoonColour = glm::vec3(0.86f, 0.9f, 0.88f);
	m_vec3MoonAmbient = glm::vec3(0.1f, 0.1f, 0.1f);
	m_vec3MoonDiffuse = glm::vec3(0.5f, 0.5f, 0.5f);
	m_vec3MoonSpecular = glm::vec3(1.0f, 1.0f, 1.0f);

//**********************************************************
// Material Properties
//**********************************************************

	//Ground
	m_vec3GroundColour = glm::vec3(0.0f, 0.0f, 0.0f);
	m_vec3GroundAmbient = glm::vec3(0.1f, 0.1f, 0.1f);
	m_vec3GroundDiffuse = glm::vec3(0.2f, 0.2f, 0.2f);	
	m_vec3GroundSpecular = glm::vec3(0.2f, 0.2f, 0.2f);
	m_fGroundShininess = 8.0f;

	//Cube
	m_vec3CubeAmbient = glm::vec3(0.1f, 0.1f, 0.1f);
	m_vec3CubeDiffuse = glm::vec3(0.2f, 0.2f, 0.2f);
	m_vec3CubeSpecular = glm::vec3(1.0f, 1.0f, 1.0f);
	m_fCubeShininess = 256.0f;

//*********************************************************************************************
// Machine Learning
//********************************************************************************************

	m_bPrevSaveState = false;
	m_bPrevRandomState = false;
	m_bPrevTrainState = false;
	m_bPrevHaltState = false;
	m_bPrevLoadState = false;
	m_bMsg = true;
	m_bCurrentMsgState = false;
	m_bRunMsg = true;
	m_bCurrentRunMsgState = false;
	sizeVal = 0.0f;

//********************************************************************************************

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//******************************************************************************************
// Matrices & Light Positions
//*******************************************************************************************
	
	//model matrix
	modelMatrix = glm::mat4(1.0f);

//*********************************************************************************************
	return true;
}

bool FiveCell::BSetupRaymarchQuad(GLuint shaderProg)
{
	float sceneVerts[] = {
		-1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f
	};
	m_uiNumSceneVerts = _countof(sceneVerts);

	unsigned int sceneIndices[] = {
		0, 1, 2,
		2, 3, 0
	};
	m_uiNumSceneIndices = _countof(sceneIndices);

	float groundRayTexCoords [] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};
	m_uiNumSceneTexCoords = _countof(groundRayTexCoords);	

	glGenVertexArrays(1, &m_uiglSceneVAO);

	glBindVertexArray(m_uiglSceneVAO);

	GLuint m_uiglSceneVBO;
	glGenBuffers(1, &m_uiglSceneVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_uiglSceneVBO);
	glBufferData(GL_ARRAY_BUFFER, m_uiNumSceneVerts * sizeof(float), sceneVerts, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	GLuint m_uiglGroundTexCoords;
	glGenBuffers(1, &m_uiglGroundTexCoords);
	glBindBuffer(GL_ARRAY_BUFFER, m_uiglGroundTexCoords);
	glBufferData(GL_ARRAY_BUFFER, m_uiNumSceneTexCoords * sizeof(float), groundRayTexCoords, GL_STATIC_DRAW);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL); 

	glGenBuffers(1, &m_uiglIndexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uiglIndexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_uiNumSceneIndices * sizeof(unsigned int), sceneIndices, GL_STATIC_DRAW);
	
	
	glBindVertexArray(0);
	glDisableVertexAttribArray(0);

	m_uiglCubeMoonDirectionLoc = glGetUniformLocation(shaderProg, "moonlight.direction");
	m_uiglCubeMoonColourLoc = glGetUniformLocation(shaderProg, "moonlight.colour");
	m_uiglCubeMoonAmbientLoc = glGetUniformLocation(shaderProg, "moonlight.ambient");
	m_uiglCubeMoonDiffuseLoc = glGetUniformLocation(shaderProg, "moonlight.diffuse");
	m_uiglCubeMoonSpecularLoc = glGetUniformLocation(shaderProg, "moonlight.specular");

	m_uiglCubeMaterialAmbientLoc = glGetUniformLocation(shaderProg, "material.ambient");
	m_uiglCubeMaterialDiffuseLoc = glGetUniformLocation(shaderProg, "material.diffuse");
	m_uiglCubeMaterialSpecularLoc = glGetUniformLocation(shaderProg, "material.specular");
	m_uiglCubeMaterialShininessLoc = glGetUniformLocation(shaderProg, "material.shininess");

	m_uiglGroundPlaneColourLoc = glGetUniformLocation(shaderProg, "ground.colour");
	m_uiglGroundPlaneAmbientLoc = glGetUniformLocation(shaderProg, "ground.ambient");
	m_uiglGroundPlaneDiffuseLoc = glGetUniformLocation(shaderProg, "ground.diffuse");
	m_uiglGroundPlaneSpecularLoc = glGetUniformLocation(shaderProg, "ground.specular");
	m_uiglGroundPlaneShininessLoc = glGetUniformLocation(shaderProg, "ground.shininess");
	
	m_gliMVEPMatrixLocation = glGetUniformLocation(shaderProg, "MVEPMat");
	m_gliInverseMVEPLocation = glGetUniformLocation(shaderProg, "InvMVEP");
	m_gliRandomSizeLocation = glGetUniformLocation(shaderProg, "randSize");
	m_gliRMSModulateValLocation = glGetUniformLocation(shaderProg, "rmsModVal");
	m_gliSineControlValLoc = glGetUniformLocation(shaderProg, "sineControlVal");

	m_uiglSkyboxTexLoc = glGetUniformLocation(shaderProg, "skyboxTex");
	m_uiglGroundTexLoc = glGetUniformLocation(shaderProg, "ground.texture");

	raymarchQuadModelMatrix = glm::mat4(1.0f);

	return true;
}

//*******************************************************************************************
// Update Stuff Here
//*******************************************************************************************
void FiveCell::update(glm::mat4 viewMat, glm::vec3 camPos, MachineLearning& machineLearning){

	//rms value from Csound
	modulateVal = *m_pRmsOut;			
	
	//matrices for raymarch shaders
	//modelViewEyeMat = eyeMat * viewMat * raymarchQuadModelMatrix;
	//inverseMVEMat = glm::inverse(modelViewEyeMat);
	//modelViewEyeProjectionMat = projMat * eyeMat * viewMat * raymarchQuadModelMatrix;
	//inverseMVEPMat = glm::inverse(modelViewEyeProjectionMat);

	glm::vec4 mengerPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	glm::mat4 mengerModelMatrix = glm::mat4(1.0f);		

	glm::vec4 posCameraSpace = viewMat * mengerModelMatrix * mengerPosition;;		

	//position of menger cube in world space
	glm::vec4 posWorldSpace = mengerModelMatrix * mengerPosition;
	
	//calculate azimuth and elevation values for hrtf
	glm::vec4 viewerPosCameraSpace = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 viewerPosWorldSpace = glm::vec4(camPos, 1.0f);;

	glm::vec4 soundPosCameraSpace = posCameraSpace;
	glm::vec4 soundPosWorldSpace = posWorldSpace;

	float rCamSpace = sqrt(pow(soundPosCameraSpace.x, 2) + pow(soundPosCameraSpace.y, 2) + pow(soundPosCameraSpace.z, 2));
		
	float rWorldSpace = sqrt(pow(soundPosWorldSpace.x - viewerPosWorldSpace.x, 2) + pow(soundPosWorldSpace.y - viewerPosWorldSpace.y, 2) + pow(soundPosWorldSpace.z - viewerPosWorldSpace.z, 2));

	//azimuth in camera space
	float valX = soundPosCameraSpace.x - viewerPosCameraSpace.x;
	float valZ = soundPosCameraSpace.z - viewerPosCameraSpace.z;

	float azimuth = atan2(valX, valZ);
	azimuth *= (180.0f/PI); 	
	
	//elevation in camera space
	float oppSide = soundPosCameraSpace.y - viewerPosCameraSpace.y;
	float sinVal = oppSide / rCamSpace;
	float elevation = asin(sinVal);
	elevation *= (180.0f/PI);		
	
	//send values to Csound pointers
	*hrtfVals[0] = (MYFLT)azimuth;
	*hrtfVals[1] = (MYFLT)elevation;
	*hrtfVals[2] = (MYFLT)rCamSpace;

	//sine function
	sineControlVal = sin(glfwGetTime());

	*m_cspSineControlVal = (MYFLT)sineControlVal;

//*********************************************************************************************
// Calculate escape values of coordinates along a ray using mandelbulb formula
// Send these values as an array to CSound to use as a spectral filter
//*********************************************************************************************

	// lay out a grid of rays centered at the origin to sample coordinates from
	std::vector<glm::vec3> rayOrigin;
	std::vector<glm::vec3> rayDirection;
	std::vector<float> step;
	bool initialRay = true;
	float interval = 0.8f;

for(int y = 0; y < NUM_RAYS; y++){

	float yPos1, yPosMinus, yPosPlus;
	if(initialRay){
		yPos1 = 0.0f;
		yPosMinus = 0.0f;
		yPosPlus = 0.0f;
	} else {
		yPosMinus = -y * interval;
		yPosPlus = y * interval;
	}
	
	for(int i = 0; i < NUM_RAYS; i++){

		float xPos1, xPosMinus, xPosPlus;
		if(initialRay){
			xPos1 = 0.0f;
			glm::vec3 rayStart = glm::vec3(xPos1, yPos1, -3.0f);
			rayOrigin.push_back(rayStart);
			glm::vec3 rayEndPointInitial = glm::vec3(xPos1, yPos1, 3.0f);
			glm::vec3 rayDiff = rayEndPointInitial - rayStart;
			glm::vec3 rayDir = glm::normalize(rayDiff);
			rayDirection.push_back(rayDir);
			float length = glm::length(rayDiff); 
			float rayStep = length / MAX_MANDEL_STEPS;
			step.push_back(rayStep);
			initialRay = false;

		} else {
			xPosMinus = -i * interval;
			xPosPlus = i * interval;

			glm::vec3 rayStartMinus = glm::vec3(xPosMinus, yPosMinus, -3.0f);
			glm::vec3 rayStartPlus = glm::vec3(xPosPlus, yPosPlus, -3.0f);

			rayOrigin.push_back(rayStartMinus);
			rayOrigin.push_back(rayStartPlus);

			glm::vec3 rayEndPointMinus = glm::vec3(xPosMinus, yPosMinus, 3.0f);
			glm::vec3 rayEndPointPlus = glm::vec3(xPosPlus, yPosPlus, 3.0f);

			glm::vec3 rayDiffMinus = rayEndPointMinus - rayStartMinus;
			glm::vec3 rayDiffPlus = rayEndPointPlus - rayStartPlus;

			glm::vec3 rayDirMinus = glm::normalize(rayDiffMinus);
			glm::vec3 rayDirPlus = glm::normalize(rayDiffPlus);

			rayDirection.push_back(rayDirMinus);
			rayDirection.push_back(rayDirPlus);

			float lengthMinus = glm::length(rayDiffMinus); 
			float lengthPlus = glm::length(rayDiffPlus); 

			float rayStepMinus = lengthMinus / MAX_MANDEL_STEPS;
			float rayStepPlus = lengthPlus / MAX_MANDEL_STEPS;

			step.push_back(rayStepMinus);
			step.push_back(rayStepPlus);

			if(yPosPlus && yPosMinus && xPosMinus && xPosPlus){
	
				glm::vec3 rayStartNxPy = glm::vec3(xPosMinus, yPosPlus, -3.0f);
				glm::vec3 rayStartPxNy = glm::vec3(xPosPlus, yPosMinus, -3.0f);

				rayOrigin.push_back(rayStartNxPy);
				rayOrigin.push_back(rayStartPxNy);

				glm::vec3 rayEndNxPy = glm::vec3(xPosMinus, yPosPlus, 3.0f);
				glm::vec3 rayEndPxNy = glm::vec3(xPosPlus, yPosMinus, 3.0f);		

				glm::vec3 rayDiffNxPy = rayEndNxPy - rayStartNxPy;
				glm::vec3 rayDiffPxNy = rayEndPxNy = rayStartPxNy;			
	
				glm::vec3 rayDirNxPy = glm::normalize(rayDiffNxPy);
				glm::vec3 rayDirPxNy = glm::normalize(rayDiffPxNy);

				rayDirection.push_back(rayDirNxPy);
				rayDirection.push_back(rayDirPxNy);

				float lengthNxPy = glm::length(rayDiffNxPy);
				float lengthPxNy = glm::length(rayDiffPxNy);	

				float rayStepNxPy = lengthNxPy / MAX_MANDEL_STEPS;
				float rayStepPxNy = lengthPxNy / MAX_MANDEL_STEPS;

				step.push_back(rayStepNxPy);
				step.push_back(rayStepPxNy);		
			}
		}
	}
}
	
	//march positions along ray
	//must be power of two in order to send to Csound table
	m_iMaxSteps = MAX_MANDEL_STEPS;
	*m_cspMaxSteps = (MYFLT)m_iMaxSteps;

	float start = 0.0;
	int iterations = 50;
	float power = 3.0f;
	float theta = 0.0f;
	float phi = 0.0f;
	float r = 0.0f;
	double count = 0.0f;
	int rayCount = rayOrigin.size();
	std::vector<std::vector<float>> escapeVals;
	std::vector<float> rays;

	// make rays vector same size as rayOrigins
	for(int i = 0; i < rayCount; i++) rays.push_back((float)i);

	// loop to step through rays
	for(int i = 0; i < rayCount; i++){

		//glm::vec3 position = rayOrigin + glm::vec3(sin(glfwGetTime()), 0.0f, 0.0f);	
		glm::vec3 position = rayOrigin[i];	

		// loop to step along the ray
		for(int j = 0; j < m_iMaxSteps; j++){

			glm::vec3 z = position;	

			// loop to execute mandelbulb formula
			for(int k = 0; k < iterations; k++){

				// mandelbulb formula adapted from 
				// https://www.shadertoy.com/view/tdtGRj
				r = length(z);
				theta = acos(z.y / r) * sineControlVal;
				phi = atan2(z.z, z.x) * sineControlVal;
				theta *= power;
				phi *= power;
				z = pow(r, power) * glm::vec3(sin(theta) * cos(phi), cos(theta), sin(phi) * sin(theta)) + position;

				count = (double)j;

				if(length(z) > 2.0f) break;

			}

			//map count value to 0 - 1 range
			count /= (double)iterations;

			//values to CSound
			rays.push_back((float)count);

			position += step[i] * rayDirection[i];
		}	
		
		escapeVals.push_back(rays);		
	}

	float avgVal = 0.0f;

	for(int i = 0; i < m_iMaxSteps; i++){

		for(int j = 0; j < rayCount; j++){

			float escVal = escapeVals[j][i];
			
			avgVal += escVal;
		}

		avgVal /= rayOrigin.size();
		*m_cspMandelEscapeVals[i] = (MYFLT)avgVal;
	}

//*********************************************************************************************
// Machine Learning 
//*********************************************************************************************
	bool currentRandomState = m_bPrevRandomState;
	if(machineLearning.bRandomParams != currentRandomState && machineLearning.bRandomParams == true){
		//random audio params
		std::uniform_real_distribution<float> distribution(1, 10);
		std::random_device rd;
		std::default_random_engine generator (rd());
		float val = distribution(generator);
		*randomFrequencyVal = (MYFLT)val;
			
		//random visual params
		std::uniform_real_distribution<float> distribution2(1.0, 4.0);
		std::random_device rd2;
		std::default_random_engine gen2 (rd2());
		sizeVal = distribution2(gen2);
	}
	m_bPrevRandomState = machineLearning.bRandomParams;

	if(machineLearning.bRecord){
		inputData.push_back((double)viewerPosWorldSpace.x);	
		inputData.push_back((double)viewerPosWorldSpace.y);	
		inputData.push_back((double)viewerPosWorldSpace.z);	

		outputData.push_back((double)*randomFrequencyVal);
		outputData.push_back((double)sizeVal);

#ifdef __APPLE__
		trainingData.recordSingleElement(inputData, outputData);	
#elif _WIN32
		trainingData.input = inputData;
		trainingData.output = outputData;
		trainingSet.push_back(trainingData);
#endif

		std::cout << "Recording Data" << std::endl;
		inputData.clear();
		outputData.clear();
	}
	machineLearning.bRecord = false;

	bool currentTrainState = m_bPrevTrainState;
	if(machineLearning.bTrainModel != currentTrainState && machineLearning.bTrainModel == true){

#ifdef __APPLE__
		staticRegression.train(trainingData);
#elif _WIN32
		staticRegression.train(trainingSet);
#endif

		std::cout << "Model Trained" << std::endl;
	}	
	m_bPrevTrainState = machineLearning.bTrainModel;

#ifdef __APPLE__
	bool currentHaltState = m_bPrevHaltState;
	if(machineLearning.bRunModel && !machineLearning.bHaltModel){
		std::vector<double> modelOut;
		std::vector<double> modelIn;

		modelIn.push_back((double)viewerPosWorldSpace.x);
		modelIn.push_back((double)viewerPosWorldSpace.y);
		modelIn.push_back((double)viewerPosWorldSpace.z);

		modelOut = staticRegression.run(modelIn);

		if(modelOut[0] > 10.0f) modelOut[0] = 10.0f;
		if(modelOut[0] < 1.0f) modelOut[0] = 1.0f;
		*randomFrequencyVal = (MYFLT)modelOut[0];
		if(modelOut[1] > 4.0f) modelOut[0] = 4.0f;
		if(modelOut[1] < 1.0f) modelOut[0] = 1.0f;
		sizeVal = (float)modelOut[1];
		std::cout << "Model Running" << std::endl;
		modelIn.clear();
		modelOut.clear();
	} else if(!machineLearning.bRunModel && machineLearning.bHaltModel != currentHaltState){
		machineLearning.bRunModel = false;
		std::cout << "Model Stopped" << std::endl;
	}
	m_bPrevHaltState = machineLearning.bHaltModel;
#elif _WIN32
	if(machineLearning.bRunModel){
		std::vector<double> modelOut;
		std::vector<double> modelIn;

		modelIn.push_back((double)viewerPosWorldSpace.x);
		modelIn.push_back((double)viewerPosWorldSpace.y);
		modelIn.push_back((double)viewerPosWorldSpace.z);

		modelOut = staticRegression.run(modelIn);

		if(modelOut[0] > 10.0f) modelOut[0] = 10.0f;
		if(modelOut[0] < 1.0f) modelOut[0] = 1.0f;
		*randomFrequencyVal = (MYFLT)modelOut[0];
		if(modelOut[1] > 4.0f) modelOut[0] = 4.0f;
		if(modelOut[1] < 1.0f) modelOut[0] = 1.0f;
		sizeVal = (float)modelOut[1];

		bool prevRunMsgState = m_bCurrentRunMsgState;
		if(m_bRunMsg != prevRunMsgState && m_bRunMsg == true){
			std::cout << "Model Running" << std::endl;
			m_bRunMsg = !m_bRunMsg;
		}
		m_bCurrentRunMsgState = m_bRunMsg;

		modelIn.clear();
		modelOut.clear();
		m_bMsg = true;
	} else if(!machineLearning.bRunModel){
		bool prevMsgState = m_bCurrentMsgState;
		if(m_bMsg != prevMsgState && m_bMsg == true){
			std::cout << "Model Stopped" << std::endl;
			m_bMsg = !m_bMsg;
		}
		m_bCurrentMsgState = m_bMsg;
		m_bRunMsg = true;
	}
#endif
		
	std::string mySavedModel = "mySavedModel.json";
	bool currentSaveState = m_bPrevSaveState;
#ifdef __APPLE__
	if(machineLearning.bSaveTrainingData!= currentSaveState && machineLearning.bSaveTrainingData == true){

		trainingData.writeJSON(mySavedModel);	

		std::cout << "Saving Training Data" << std::endl;
	}
	m_bPrevSaveState = machineLearning.bSaveTrainingData;
#elif _WIN32
	if(machineLearning.bSaveModel!= currentSaveState && machineLearning.bSaveModel == true){

		staticRegression.writeJSON(mySavedModel);
		std::cout << "Saving Training Data" << std::endl;
	}
	m_bPrevSaveState = machineLearning.bSaveModel;
#endif

	bool currentLoadState = m_bPrevLoadState;
#ifdef __APPLE__
	if(machineLearning.bLoadTrainingData != currentLoadState && machineLearning.bLoadTrainingData == true){
	
		trainingData.readJSON(mySavedModel);
		staticRegression.train(trainingData);

		std::cout << "Loading Data and Training Model" << std::endl;
	}
	m_bPrevLoadState = machineLearning.bLoadTrainingData;
#elif _WIN32
	if(machineLearning.bLoadModel != currentLoadState && machineLearning.bLoadModel == true){
	
		staticRegression.readJSON(mySavedModel);	

		std::cout << "Loading Data and Training Model" << std::endl;
	}
	m_bPrevLoadState = machineLearning.bLoadModel;
#endif

//*********************************************************************************************
}

//*********************************************************************************************
// Draw Stuff Here
//*********************************************************************************************
void FiveCell::draw(glm::mat4 projMat, glm::mat4 viewMat, glm::mat4 eyeMat, RaymarchData& raymarchData, GLuint mengerProg){
		
	//matrices for raymarch shaders
	modelViewEyeMat = eyeMat * viewMat * raymarchQuadModelMatrix;
	inverseMVEMat = glm::inverse(modelViewEyeMat);
	modelViewEyeProjectionMat = projMat * eyeMat * viewMat * raymarchQuadModelMatrix;
	inverseMVEPMat = glm::inverse(modelViewEyeProjectionMat);

	glm::mat4 viewEyeMat = eyeMat * viewMat;
	
	camPosPerEye = glm::vec3(viewEyeMat[0][3], viewEyeMat[1][3], viewEyeMat[2][3]);
	
	//draw glass mandelbulb -----------------------------------------------------------------
	float mengerAspect = raymarchData.aspect;
	float mengerTanFovYOver2 = raymarchData.tanFovYOver2;

	glBindVertexArray(m_uiglSceneVAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uiglIndexBuffer);

	glUseProgram(mengerProg);

	glUniform1i(m_uiglSkyboxTexLoc, 0);
	glUniform1i(m_uiglGroundTexLoc, 1);
	glUniformMatrix4fv(m_gliMVEPMatrixLocation, 1, GL_FALSE, &modelViewEyeProjectionMat[0][0]);
	glUniformMatrix4fv(m_gliInverseMVEPLocation, 1, GL_FALSE, &inverseMVEPMat[0][0]);

	glUniform3f(m_uiglCubeMoonDirectionLoc, m_vec3MoonDirection.x, m_vec3MoonDirection.y, m_vec3MoonDirection.z);
	glUniform3f(m_uiglCubeMoonColourLoc, m_vec3MoonColour.x, m_vec3MoonColour.y, m_vec3MoonColour.z);
	glUniform3f(m_uiglCubeMoonAmbientLoc, m_vec3MoonAmbient.x, m_vec3MoonAmbient.y, m_vec3MoonAmbient.z);
	glUniform3f(m_uiglCubeMoonDiffuseLoc, m_vec3MoonDiffuse.x, m_vec3MoonDiffuse.y, m_vec3MoonDiffuse.z);
	glUniform3f(m_uiglCubeMoonSpecularLoc, m_vec3MoonSpecular.x, m_vec3MoonSpecular.y, m_vec3MoonSpecular.z);

	glUniform3f(m_uiglCubeMaterialAmbientLoc, m_vec3CubeAmbient.x, m_vec3CubeAmbient.y, m_vec3CubeAmbient.z);
	glUniform3f(m_uiglCubeMaterialDiffuseLoc, m_vec3CubeDiffuse.x, m_vec3CubeDiffuse.y, m_vec3CubeDiffuse.z);
	glUniform3f(m_uiglCubeMaterialSpecularLoc, m_vec3CubeSpecular.x, m_vec3CubeSpecular.y, m_vec3CubeSpecular.z);
	glUniform1f(m_uiglCubeMaterialShininessLoc, m_fCubeShininess);

	glUniform3f(m_uiglGroundPlaneColourLoc, m_vec3GroundColour.x, m_vec3GroundColour.y, m_vec3GroundColour.z);
	glUniform3f(m_uiglGroundPlaneAmbientLoc, m_vec3GroundAmbient.x, m_vec3GroundAmbient.y, m_vec3GroundAmbient.z);
	glUniform3f(m_uiglGroundPlaneDiffuseLoc, m_vec3GroundDiffuse.x, m_vec3GroundDiffuse.y, m_vec3GroundDiffuse.z);
	glUniform3f(m_uiglGroundPlaneSpecularLoc, m_vec3GroundSpecular.x, m_vec3GroundSpecular.y, m_vec3GroundSpecular.z);
	glUniform1f(m_uiglGroundPlaneShininessLoc, m_fGroundShininess);
	
	glUniform1f(m_gliRandomSizeLocation, sizeVal);
	glUniform1f(m_gliRMSModulateValLocation, modulateVal);
	glUniform1f(m_gliSineControlValLoc, sineControlVal);
	
	glDrawElements(GL_TRIANGLES, m_uiNumSceneIndices * sizeof(unsigned int), GL_UNSIGNED_INT, (void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}


void FiveCell::exit(){
	//stop csound
	session->StopPerformance();
	//close GL context and any other GL resources
	glfwTerminate();
}
