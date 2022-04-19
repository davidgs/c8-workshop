package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strings"

	"github.com/camunda-cloud/zeebe/clients/go/pkg/entities"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/pb"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/worker"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/zbc"
	"gopkg.in/yaml.v2"

	"github.com/gorilla/mux"
)

type ClientEnv struct {
	ZeebeAddress      string `yaml:"zeebeAddress"`
	ZeebeClientID    string `yaml:"zeebeClientID"`
	ZeebeClientSecret string `yaml:"zeebeClientSecret"`
	ZeebeAuthServer   string `yaml:"zeebeAuthServer"`
	ProcessID				 string `yaml:"processID"`
	Variables				 map[string]interface{} `yaml:"variables"`
}

var OCP = zbc.OAuthProviderConfig{
	ClientID:               config.ZeebeClientID,
	ClientSecret:           config.ZeebeClientSecret,
	Audience:               strings.Split(config.ZeebeAddress, ":")[0],
	AuthorizationServerURL: config.ZeebeAuthServer,
	Cache:                  nil,
	Timeout:                0,
}

var config = ClientEnv{}
var readyClose = make(chan struct{})

type App struct {
	Router *mux.Router
}

func init_proc() {
	dat, err := ioutil.ReadFile("./zeebe.yaml")
	if err != nil {
		log.Fatal("No startup file: ", err)
	}
	err = yaml.Unmarshal(dat, &config)
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	fmt.Println("Starting Camunda Cloud Zeebe Client")
	fmt.Println("===================================")
	a := App{}
	err := a.Initialize()
	if err != nil {
		fmt.Println("Error:", err)
		log.Fatal(err)
	}
	// init_proc()

	config.ZeebeAddress = os.Getenv("ZEEBE_ADDRESS")
	if config.ZeebeAddress == "" {
		init_proc()
		// os.Setenv("ZEEBE_ADDRESS", config.ZeebeAddress)
		// os.Setenv("ZEEBE_CLIENT_ID", config.ZeebeClientID)
		// os.Setenv("ZEEBE_CLIENT_SECRET", config.ZeebeClientSecret)
		// os.Setenv("ZEEBE_AUTH_SERVER", config.ZeebeAuthServer)
	}
a.InitializeRoutes()
	fmt.Println("Server Started, listening on port 5050")
	a.Run(":5050")

}

func (a *App) Initialize() error {
	a.Router = mux.NewRouter().StrictSlash(true)
	return nil
}

//ActivateJobs
func (a *App) InitializeRoutes() {
	a.Router.HandleFunc("/Topology", a.getTopology).Methods("OPTIONS", "POST")
	fmt.Println("Started POST /Topology")
	a.Router.HandleFunc("/Topology", a.getTopology).Methods("GET")
	fmt.Println("Started GET /Topology")
	a.Router.HandleFunc("/CreateInstance", a.createInstance).Methods("OPTIONS", "POST")
	fmt.Println("Started POST /CreateInstance")
}

func (a *App) Run(addr string) {
	fmt.Println("Running ... ")
	// credentials := handlers.AllowCredentials()
	// //handlers.IgnoreOptions()
	// handlers.AllowedHeaders([]string{"X-Requested-With", "Content-Type", "Authorization", "Referer", "Origin"})
	// methods := handlers.AllowedMethods([]string{"POST", "GET", "OPTIONS"})
	// origins := handlers.AllowedOriginValidator(originValidator)
	fileServer := http.FileServer(http.Dir("./pix")) // New code
    http.Handle("/pix", fileServer)
	log.Fatal(http.ListenAndServe(addr, a.Router))

}

func (a *App) createInstance(w http.ResponseWriter, r *http.Request) {
	fmt.Println("startInstance")
	body, err := ioutil.ReadAll(r.Body)
	if err != nil {
		fmt.Println(err)
	}
	env := ClientEnv{}
	err = json.Unmarshal(body, &env)
	if err != nil {
		fmt.Println(err)
	}
	authCreds := zbc.OAuthProviderConfig{
		ClientID:               env.ZeebeClientID,
		ClientSecret:           env.ZeebeClientSecret,
		Audience:               strings.Split(env.ZeebeAddress, ":")[0],
		AuthorizationServerURL: env.ZeebeAuthServer,
		Cache:                  nil,
		Timeout:                0,
	}
	fmt.Println("Zeebe Address: ", env.ZeebeAddress)
	fmt.Println("Zeebe Client ID: ", env.ZeebeClientID)
	fmt.Println("Zeebe Client Secret: ", env.ZeebeClientSecret)
	fmt.Println("Zeebe Auth Server: ", env.ZeebeAuthServer)
	fmt.Println("Zeebe Auth Creds: ", authCreds)
	fmt.Println("ZeeBe Config: ", authCreds.Audience)
	fmt.Println("Config Address: ", config.ZeebeAddress)
	OAuthCredentialsProvider, err := zbc.NewOAuthCredentialsProvider(&authCreds)
	if err != nil {
		fmt.Println(err)
		return;
	}
	clientConfig := zbc.ClientConfig{
		GatewayAddress:      env.ZeebeAddress,
		CredentialsProvider: OAuthCredentialsProvider,
	}
	client, err := zbc.NewClient(&clientConfig)
	if err != nil {
		fmt.Println(err)
		return
	}
	dat, err := os.ReadFile("./C.3.0.png")
	if err != nil {
		fmt.Println(err)
		return
	}
	err = os.WriteFile("/pix/C.3.0.png", dat, 0644)
	ctx := context.Background()

	 variables := env.Variables
	 variables["image"] = "http://localhost:5050/pix/C.3.0.png"
	// variables["orderId"] = "31243"
	request, err := client.NewCreateInstanceCommand().BPMNProcessId(env.ProcessID).LatestVersion().VariablesFromMap(variables)
	if err != nil {
		fmt.Println(err)
	}
	msg, err := request.Send(ctx)
	if err != nil {
		fmt.Println(err)
	}

	fmt.Println(msg.String())
	// jobWorker := client.NewJobWorker().JobType("fetch_data").Handler(getData).Open()

	// <-readyClose
	// jobWorker.Close()
	// jobWorker.AwaitClose()
	// var instance = entities.NewWorkflowInstance()
	// instance.BpmnProcessId = "orderProcess"
	// instance.Variables = map[string]interface{}{
	// 	"orderId": "123",
	// }
	// ctx := context.Background()
	// resp, err := client.NewCreateWorkflowInstanceCommand().WorkflowInstance(instance).Send(ctx)
	// if err != nil {
	// 	fmt.Println(err)
	// }
	// fmt.Println(resp)
}

func (a *App) getTopology(w http.ResponseWriter, r *http.Request) {

	body, err := ioutil.ReadAll(r.Body)
	if err != nil {
		fmt.Println(err)
	}
	env := ClientEnv{}
	err = json.Unmarshal(body, &env)
	if err != nil {
		fmt.Println(err)
	}
	authCreds := zbc.OAuthProviderConfig{
		ClientID:               env.ZeebeClientID,
		ClientSecret:           env.ZeebeClientSecret,
		Audience:               strings.Split(env.ZeebeAddress, ":")[0],
		AuthorizationServerURL: env.ZeebeAuthServer,
		Cache:                  nil,
		Timeout:                0,
	}
	fmt.Println("Zeebe Address: ", env.ZeebeAddress)
	fmt.Println("Zeebe Client ID: ", env.ZeebeClientID)
	fmt.Println("Zeebe Client Secret: ", env.ZeebeClientSecret)
	fmt.Println("Zeebe Auth Server: ", env.ZeebeAuthServer)
	fmt.Println("Zeebe Auth Creds: ", authCreds)
	fmt.Println("ZeeBe Config: ", authCreds.Audience)
	fmt.Println("Config Address: ", config.ZeebeAddress)
	OAuthCredentialsProvider, err := zbc.NewOAuthCredentialsProvider(&authCreds)
	if err != nil {
		fmt.Println(err)
		return;
	}
	clientConfig := zbc.ClientConfig{
		GatewayAddress:      env.ZeebeAddress,
		CredentialsProvider: OAuthCredentialsProvider,
	}
	client, err := zbc.NewClient(&clientConfig)
	if err != nil {
		fmt.Println(err)
		return
	}

	ctx := context.Background()
	topology, err := client.NewTopologyCommand().Send(ctx)
	if err != nil {
		fmt.Println(err)
		return
	}
	fmt.Printf("%+v\n", topology.String())
	fmt.Printf("Brokers: %+v\n", topology.Brokers)
	for _, broker := range topology.Brokers {
		fmt.Println("Broker", broker.Host, ":", broker.Port)
		for _, partition := range broker.Partitions {
			fmt.Println("  Partition", partition.PartitionId, ":", roleToString(partition.Role))
		}
	}
}

func getData(client worker.JobClient, job entities.Job) {
	fmt.Println("getData")
	jobKey := job.GetKey()

	headers, err := job.GetCustomHeadersAsMap()
	if err != nil {
		// failed to handle job as we require the custom job headers
		failJob(client, job)
		return
	}

	variables, err := job.GetVariablesAsMap()
	if err != nil {
		// failed to handle job as we require the variables
		failJob(client, job)
		return
	}

	variables["totalPrice"] = 46.50
	request, err := client.NewCompleteJobCommand().JobKey(jobKey).VariablesFromMap(variables)
	if err != nil {
		// failed to set the updated variables
		failJob(client, job)
		return
	}

	log.Println("Complete job", jobKey, "of type", job.Type)
	log.Println("Processing order:", variables["orderId"])
	log.Println("Collect money using payment method:", headers["method"])

	ctx := context.Background()
	_, err = request.Send(ctx)
	if err != nil {
		fmt.Println(err)
	}

	log.Println("Successfully completed job")
	close(readyClose)
}

func failJob(client worker.JobClient, job entities.Job) {
	log.Println("Failed to complete job", job.GetKey())

	ctx := context.Background()
	_, err := client.NewFailJobCommand().JobKey(job.GetKey()).Retries(job.Retries - 1).Send(ctx)
	if err != nil {
		fmt.Println(err)
	}
}

func roleToString(role pb.Partition_PartitionBrokerRole) string {
	switch role {
	case pb.Partition_LEADER:
		return "Leader"
	case pb.Partition_FOLLOWER:
		return "Follower"
	default:
		return "Unknown"
	}
}