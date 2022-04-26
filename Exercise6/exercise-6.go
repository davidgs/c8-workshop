/**
 * MIT License
 *
 * Copyright (c) 2022 David G. Simmons
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"

	log "github.com/sirupsen/logrus"

	"github.com/camunda-cloud/zeebe/clients/go/pkg/entities"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/worker"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/zbc"
	"gopkg.in/yaml.v2"

	vision "cloud.google.com/go/vision/apiv1"
	opts "google.golang.org/api/option"
)

// Set this to the name of your task
const PROC_NAME = "DoPictureAI"

// Set this to `false` to stop output to the terminal
const DEBUG = true

// ENV sucture for all the Camunda Platform 8 credentials and settings
type ENV struct {
	ZeebeAddress      string `yaml:"zeebeAddress"`
	ZeebeClientID     string `yaml:"zeebeClientID"`
	ZeebeClientSecret string `yaml:"zeebeClientSecret"`
	ZeebeAuthServer   string `yaml:"zeebeAuthServer"`
}

// App Structure
type App struct {
}

type interestingAnnotations struct {
	DetectionConfidence    float64 `json:"detection_confidence"`
	LandmarkingConfidence  float64 `json:"landmarking_confidence"`
	JoyLikelihood          int     `json:"joy_likelihood"`
	SorrowLikelihood       int     `json:"sorrow_likelihood"`
	AngerLikelihood        int     `json:"anger_likelihood"`
	SurpriseLikelihood     int     `json:"surprise_likelihood"`
	UnderExposedLikelihood int     `json:"under_exposed_likelihood"`
	BlurredLikelihood      int     `json:"blurred_likelihood"`
	HeadwearLikelihood     int     `json:"headwear_likelihood"`
}

type Emotions struct {
	Joy            string
	JoyNumber      int
	Anger          string
	AngerNumber    int
	Surprise       string
	SurpriseNumber int
	Sorrow         string
	SorrowNumber   int
	IsPicture      bool
}

// StatusError represents an error with an associated HTTP status code.
type StatusError struct {
	Code int
	Err  error
}

// JobVars the variables we get from the Camunda Platform 8 process
type JobVars struct {
	ImageLoc   string `json:"imageLoc"`
	IsPicture bool `json:"isPicture"`
}

var config = ENV{}
var readyClose = make(chan struct{})


func main() {
	a := App{}
	dPrintln("Starting Camunda Cloud Exercise 6 Worker")
	dPrintln("===================================")
	err := a.Initialize()
	if err != nil {
		dPrintln("Error:", err)
		log.Fatal(err)
	}
}

// read the config file and store settings
func (a *App) init_proc() {
	dat, err := ioutil.ReadFile("../zeebe.yaml")
	if err != nil {
		log.Fatal("No startup file: ", err)
	}
	err = yaml.Unmarshal(dat, &config)
	if err != nil {
		log.Fatal(err)
	}
}

// Initialize the Camunda Platform 8 client
func (a *App) Initialize() error {
	config.ZeebeAddress = os.Getenv("ZEEBE_ADDRESS")
	if config.ZeebeAddress == "" {
		a.init_proc()
		os.Setenv("ZEEBE_ADDRESS", config.ZeebeAddress)
		os.Setenv("ZEEBE_CLIENT_ID", config.ZeebeClientID)
		os.Setenv("ZEEBE_CLIENT_SECRET", config.ZeebeClientSecret)
		os.Setenv("ZEEBE_AUTH_SERVER", config.ZeebeAuthServer)
	}
	client, err := zbc.NewClient(&zbc.ClientConfig{
		GatewayAddress: config.ZeebeAddress,
	})

	if err != nil {
		panic(err)
	}
	// Start the job worker to handle jobs
	jobWorker := client.NewJobWorker().JobType(PROC_NAME).Handler(a.handleC8Job).Open()

	<-readyClose
	jobWorker.Close()
	jobWorker.AwaitClose()

	return nil
}

// Here's where we handle incoming script task jobs.
func (a *App) handleC8Job(client worker.JobClient, job entities.Job) {
	dPrintln("handleC8Job")
	jobKey := job.GetKey()
	_, err := job.GetCustomHeadersAsMap()
	if err != nil {
		// failed to handle job as we require the custom job headers
		failJob(client, job)
		return
	}
	jobVars := JobVars{}
	err = job.GetVariablesAs(&jobVars)
	if err != nil {
		failJob(client, job)
		return
	}
	dPrintf("%+v\n", jobVars)
	if jobVars.ImageLoc == "" {
		failJob(client, job)
		return
	}
	// Get the image from the storage
	resp, err := http.Get(jobVars.ImageLoc)
	if err != nil {
		failJob(client, job)
		return
	}
	defer resp.Body.Close()
	emote := doAIOnPic(&resp.Body)
	dPrintln(emote)

	variables := make(map[string]interface{})
	variables["isPicture"] = emote.IsPicture
	variables["Joy"] = emote.Joy
	variables["JoyNumber"] = emote.JoyNumber
	variables["Anger"] = emote.Anger
	variables["AngerNumber"] = emote.AngerNumber
	variables["Surprise"] = emote.Surprise
	variables["SurpriseNumber"] = emote.SurpriseNumber
	variables["Sorrow"] = emote.Sorrow
	variables["SorrowNumber"] = emote.SorrowNumber
	dPrintf("%+v\n", variables)
	request, err := client.NewCompleteJobCommand().JobKey(jobKey).VariablesFromMap(variables)
	if err != nil {
		failJob(client, job)
		return
	}
	dPrintln("Complete job", jobKey, "of type", job.Type)
	ctx := context.Background()
	_, err = request.Send(ctx)
	if err != nil {
		panic(err)
	}
	dPrintln("Successfully completed job")
}



// Allows StatusError to satisfy the error interface.
func (se StatusError) Error() string {
	return se.Err.Error()
}

// Status Returns our HTTP status code.
func (se StatusError) Status() int {
	return se.Code
}

func doAIOnPic(image *io.ReadCloser) Emotions { // (string, error) {
	ctx := context.Background()
	options := opts.WithCredentialsFile("../google.json")
	client, err := vision.NewImageAnnotatorClient(ctx, options)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()
	im, err := vision.NewImageFromReader(io.Reader(*image))
	if err != nil {
		log.Fatalf("Failed to create image: %v", err)
	}
	emotions := Emotions{}
	annotations, err := client.DetectFaces(ctx, im, nil, 10)
	if err != nil {
		dPrintln("No faces found.")
		emotions.IsPicture = false
		log.Fatalf("Failed to detect faces: %v", err)
	}
	// no people in this photo!
	if len(annotations) == 0 {
		dPrintln("No faces found.")
		emotions.IsPicture = false
	} else {
		emotions.IsPicture = true
		dPrintln("Faces:")
		for i, annotation := range annotations {
			goodStuff := interestingAnnotations{}
			data, err := json.Marshal(annotation)
			if err != nil {
				log.Fatal(err)
			}
			err = json.Unmarshal(data, &goodStuff)
			if err != nil {
				log.Fatal(err)
			}
			dPrintln("  Face", i)
			emotions.Anger = annotation.AngerLikelihood.String()
			emotions.AngerNumber = goodStuff.AngerLikelihood
			dPrintln("    Anger:", annotation.AngerLikelihood)
			emotions.Joy = annotation.JoyLikelihood.String()
			emotions.JoyNumber = goodStuff.JoyLikelihood
			dPrintln("    Joy:", annotation.JoyLikelihood)
			emotions.Surprise = annotation.SurpriseLikelihood.String()
			emotions.SurpriseNumber = goodStuff.SurpriseLikelihood
			dPrintln("    Surprise:", annotation.SurpriseLikelihood)
			emotions.Sorrow = annotation.SorrowLikelihood.String()
			emotions.SorrowNumber = goodStuff.SorrowLikelihood
			dPrintln("    Sorrow:", annotation.SorrowLikelihood)
		}
	}
	return emotions
}
// If we fail to handle the job for some reason.
func failJob(client worker.JobClient, job entities.Job) {
	dPrintln("Failed to complete job")
	job.GetKey()
	ctx := context.Background()
	_, err := client.NewFailJobCommand().JobKey(job.GetKey()).Retries(job.Retries - 1).Send(ctx)
	if err != nil {
		panic(err)
	}
}

func dPrintf(format string, a ...interface{}) {
	if DEBUG {
		fmt.Printf(format, a...)
	}
}

func dPrintln(a ...interface{}) {
	if DEBUG {
		fmt.Println(a...)
	}
}