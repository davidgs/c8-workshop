package main

import (
	"context"
	"crypto/sha1"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"

	"github.com/camunda-cloud/zeebe/clients/go/pkg/pb"
	"github.com/camunda-cloud/zeebe/clients/go/pkg/zbc"

	"github.com/gorilla/mux"
)

// all the client variables are kept here.
type ClientEnv struct {
	ZeebeAddress      string                 `yaml:"zeebeAddress"`
	ZeebeClientID     string                 `yaml:"zeebeClientID"`
	ZeebeClientSecret string                 `yaml:"zeebeClientSecret"`
	ZeebeAuthServer   string                 `yaml:"zeebeAuthServer"`
	ProcessID         string                 `yaml:"processID"`
	Variables         map[string]interface{} `yaml:"variables"`
}

// Oauth Provider details
var OCP = zbc.OAuthProviderConfig{
	ClientID:               config.ZeebeClientID,
	ClientSecret:           config.ZeebeClientSecret,
	Audience:               strings.Split(config.ZeebeAddress, ":")[0],
	AuthorizationServerURL: config.ZeebeAuthServer,
	Cache:                  nil,
	Timeout:                0,
}

var config = ClientEnv{}

// Set this to `false` to stop output to the terminal
const DEBUG = true

// set these yourself as needed
const SERVER = "YOUR_SERVER" // eg 'myhost.myserver.com'
const PORT = 5050 // any port will do
const SECURE = true
const CERT_PATH = "/full/path/to/your/cert.pem"
const KEY_PATH = "/full/path/to/your/key.pem"
const PIX_PATH = "/pix/" // any path will do, be aware that it will be used as `./dir/` so don't no absolute path

type App struct {
	Router *mux.Router
}

func main() {
	dPrintln("Starting Camunda Cloud Process Broker")
	dPrintln("===================================")
	a := App{}
	err := a.Initialize()
	if err != nil {
		dPrintln("Error:", err)
		log.Fatal(err)
	}
	a.InitializeRoutes()
	dPrintln("Server Started, listening on port ", PORT)
	a.Run(":" + strconv.FormatInt(PORT, 10))
}

func (a *App) Initialize() error {
	a.Router = mux.NewRouter().StrictSlash(true)
	return nil
}

//Initialize all the routes to serve
func (a *App) InitializeRoutes() {
	a.Router.PathPrefix(PIX_PATH).Handler(http.StripPrefix(PIX_PATH,
     http.FileServer(http.Dir("." + PIX_PATH))))
	a.Router.HandleFunc("/Topology", a.getTopology).Methods("OPTIONS", "POST")
	dPrintln("Started POST /Topology")
	a.Router.HandleFunc("/Topology", a.getTopology).Methods("GET")
	dPrintln("Started GET /Topology")
	a.Router.HandleFunc("/CreateInstance", a.createInstance).Methods("OPTIONS", "POST")
	dPrintln("Started POST /CreateInstance")
}

func (a *App) Run(addr string) {
	dPrintln("Running ... ")
	if SECURE {
		log.Fatal(http.ListenAndServeTLS(addr, CERT_PATH, KEY_PATH, a.Router))
	} else {
	// for no SSL, comment out above and uncomment below
		log.Fatal(http.ListenAndServe(addr, a.Router))
	}

}

// createInstance will create a process instance pased on incoming data
func (a *App) createInstance(w http.ResponseWriter, r *http.Request) {
	dPrintln("startInstance")
	if r.Method == "GET" { // GET outta here! :-)
		log.Println("GET Method Not Supported")
		http.Error(w, "GET Method not supported", 400)
	} else {
		var filename string
		env := ClientEnv{}
		reader, err := r.MultipartReader()
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		for {
			part, err := reader.NextPart()
			if err == io.EOF {
				break
			}
			switch part.FormName() {
			case "image_file":
				// function body of a http.HandlerFunc
				// get the image from the request
				cont, err := ioutil.ReadAll(part)
				if err != nil {
					http.Error(w, err.Error(), http.StatusBadRequest)
					return
				}
				data, err := base64.StdEncoding.DecodeString(string(cont))
				if err != nil {
					log.Fatal("error:", err)
					dPrintln(err)
					http.Error(w, err.Error(), http.StatusInternalServerError)
					return
				}
				fname := fmt.Sprintf(".%s%x", PIX_PATH, sha1.Sum([]byte(data)))
				newFile, err := os.Create(fname + ".jpg")
				if err != nil {
					dPrintln(err)
					http.Error(w, err.Error(), http.StatusInternalServerError)
					return
				}
				if SECURE {
					filename = fmt.Sprintf("https://%s:%d%s%x.jpg", SERVER, PORT, PIX_PATH, sha1.Sum([]byte(data)))
				} else {
					filename = fmt.Sprintf("http://%s:%d%s%x.jpg", SERVER, PORT, PIX_PATH, sha1.Sum([]byte(data)))
				}
				defer newFile.Close()
				// // write the content to the new file
				_, err = newFile.Write(data)
				if err != nil {
					dPrintln(err)
					http.Error(w, err.Error(), http.StatusInternalServerError)
					return
				}
			case "credentials":
				cred, err := ioutil.ReadAll(part)
				if err != nil {
					dPrintln(err)
					http.Error(w, err.Error(), http.StatusInternalServerError)
					return
				}
				err = json.Unmarshal([]byte(cred), &env)
				if err != nil {
					dPrintln(err)
					http.Error(w, err.Error(), http.StatusInternalServerError)
					return
				}
			}
		}
		authCreds := zbc.OAuthProviderConfig{
			ClientID:               env.ZeebeClientID,
			ClientSecret:           env.ZeebeClientSecret,
			Audience:               strings.Split(env.ZeebeAddress, ":")[0],
			AuthorizationServerURL: env.ZeebeAuthServer,
		}
		OAuthCredentialsProvider, err := zbc.NewOAuthCredentialsProvider(&authCreds)
		if err != nil {
			dPrintln(err)
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		clientConfig := zbc.ClientConfig{
			GatewayAddress:      env.ZeebeAddress,
			CredentialsProvider: OAuthCredentialsProvider,
		}
		client, err := zbc.NewClient(&clientConfig)
		if err != nil {
			dPrintln(err)
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		ctx := context.Background()
		if filename != "" {
			env.Variables["imageLoc"] =  filename
			variables := env.Variables
			request, err := client.NewCreateInstanceCommand().BPMNProcessId(env.ProcessID).LatestVersion().VariablesFromMap(variables)
			if err != nil {
				dPrintln(err)
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			_, err = request.Send(ctx)
			if err != nil {
				dPrintln(err)
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
		}
	}
}

func (a *App) getTopology(w http.ResponseWriter, r *http.Request)  {

	body, err := ioutil.ReadAll(r.Body)
	if err != nil {
		dPrintln(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	env := ClientEnv{}
	err = json.Unmarshal(body, &env)
	if err != nil {
		dPrintln(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	authCreds := zbc.OAuthProviderConfig{
		ClientID:               env.ZeebeClientID,
		ClientSecret:           env.ZeebeClientSecret,
		Audience:               strings.Split(env.ZeebeAddress, ":")[0],
		AuthorizationServerURL: env.ZeebeAuthServer,
		Cache:                  nil,
		Timeout:                0,
	}
	dPrintln("Zeebe Address: ", env.ZeebeAddress)
	dPrintln("Zeebe Client ID: ", env.ZeebeClientID)
	dPrintln("Zeebe Client Secret: ", env.ZeebeClientSecret)
	dPrintln("Zeebe Auth Server: ", env.ZeebeAuthServer)
	dPrintln("Zeebe Auth Creds: ", authCreds)
	dPrintln("ZeeBe Config: ", authCreds.Audience)
	dPrintln("Config Address: ", config.ZeebeAddress)
	OAuthCredentialsProvider, err := zbc.NewOAuthCredentialsProvider(&authCreds)
	if err != nil {
		dPrintln(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	clientConfig := zbc.ClientConfig{
		GatewayAddress:      env.ZeebeAddress,
		CredentialsProvider: OAuthCredentialsProvider,
	}
	client, err := zbc.NewClient(&clientConfig)
	if err != nil {
		dPrintln(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	ctx := context.Background()
	topology, err := client.NewTopologyCommand().Send(ctx)
	if err != nil {
		dPrintln(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	dPrintf("%+v\n", topology.String())
	dPrintf("Brokers: %+v\n", topology.Brokers)
	for _, broker := range topology.Brokers {
		dPrintln("Broker", broker.Host, ":", broker.Port)
		for _, partition := range broker.Partitions {
			dPrintln("  Partition", partition.PartitionId, ":", roleToString(partition.Role))
		}
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