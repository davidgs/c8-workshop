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
	"fmt"
	"github.com/gorilla/handlers"
	"github.com/gorilla/mux"
	log "github.com/sirupsen/logrus"
	"io/ioutil"
	"net/http"
	"os"

	"github.com/camunda-cloud/zeebe/clients/go/pkg/entities"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/worker"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/zbc"
	"gopkg.in/yaml.v2"
)

type ENV struct {
	ZeebeAddress      string `yaml:"zeebeAddress"`
	ZeebeClientID     string `yaml:"zeebeClientID"`
	ZeebeClientSecret string `yaml:"zeebeClientSecret"`
	ZeebeAuthServer   string `yaml:"zeebeAuthServer"`
}

type App struct {
	Router *mux.Router
}

type JobVars struct {
	Add   int `json:"add"`
	Count int `json:"count"`
}

var config = ENV{}
var readyClose = make(chan struct{})

func main() {
	a := App{}
	fmt.Println("Starting Camunda Cloud Zeebe ScriptWorker")
	fmt.Println("===================================")
	err := a.Initialize()
	if err != nil {
		fmt.Println("Error:", err)
		log.Fatal(err)
	}
	a.InitializeRoutes()
	fmt.Println("Server Started")
	a.Run(":4040")

}

func (a *App) InitializeRoutes() {
	a.Router.HandleFunc("/", a.handleSlash).Methods("OPTIONS", "POST", "GET")
}

func (a *App) Run(addr string) {
	fmt.Println("Running ... ")
	credentials := handlers.AllowCredentials()
	handlers.AllowedHeaders([]string{"X-Requested-With", "Content-Type", "Authorization", "Referer", "Origin"})
	methods := handlers.AllowedMethods([]string{"POST", "GET", "OPTIONS"})
	origins := handlers.AllowedOriginValidator(nil)
	log.Fatal(http.ListenAndServe(addr, handlers.CORS(credentials, methods, origins)(a.Router)))
}

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
func (a *App) Initialize() error {
	a.Router = mux.NewRouter().StrictSlash(true)
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
	jobWorker := client.NewJobWorker().JobType("AddOneTask").Handler(a.handleC8Job).Open()

	<-readyClose
	jobWorker.Close()
	jobWorker.AwaitClose()

	return nil
}

func (a *App) handleSlash(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "Go Away!")
}

func (a *App) handleC8Job(client worker.JobClient, job entities.Job) {
	fmt.Println("handleC8Job")
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

	fmt.Printf("%+v\n", jobVars)
	jobVars.Count = jobVars.Count + jobVars.Add
	fmt.Printf("%+v\n", jobVars)
	request, err := client.NewCompleteJobCommand().JobKey(jobKey).VariablesFromObject(jobVars)
	if err != nil {
		// failed to set the updated variables
		failJob(client, job)
		return
	}

	fmt.Println("Complete job", jobKey, "of type", job.Type)

	ctx := context.Background()
	_, err = request.Send(ctx)
	if err != nil {
		panic(err)
	}
	fmt.Println("Successfully completed job")
	// close(readyClose)
}

// func (a *App) startJobWorker(client worker.JobClient){
// 	jobWorker := client.NewJobWorker().JobType("AddOneTask").Handler(a.handleC8Job).Open()

//     <- readyClose
// 	jobWorker.Close()
// 	jobWorker.AwaitClose()
// }

func failJob(client worker.JobClient, job entities.Job) {
	fmt.Println("Failed to complete job", job.GetKey())

	ctx := context.Background()
	_, err := client.NewFailJobCommand().JobKey(job.GetKey()).Retries(job.Retries - 1).Send(ctx)
	if err != nil {
		panic(err)
	}
}
