# Camunda Cloud IoT Workshop

## Orchestrating IoT devices with Camunda Cloud

This is a hands-on hardware and software lab where you will learn how to build and deploy Camunda Platform 8 (cloud) models, decision tables and workers to orchestrate work from an IoT device.

## The Exercises

There are a total of 6 exercises in this lab. Each one builds on the previous one.

There is also a [`Server Process`](Server) that you can run on a host to accept the incoming requests from the IoT devices. Note that this process should be run on an internet-accessible host, or via `ngrok` or another service which allows access to the localhost.

## Agenda

- [Exercise 1: Sign up for accounts](Exercise1/index.md)
  - Camunda Cloud Account
    - Provision a cluster
    - Get your API Keys
  - Google Image AI
    - Add the Vision API
    - Get your API Keys
- [Exercise 2: Your first process](Exercise2/index.md)
  - Create simple Golang task worker
    - Start `script-worker.go`
  - Create your first model
    - Create your first process
  - Test the Script Worker
- [Exercise 3: Building IoT Hardware](Exercise3/index.md)
  - Build hardware
  - Flash the firmware
  - Configure the hardware
    - Connect to WiFi
    - Save Credentials (use from [step 1](Exercise1/index.md))
- [Exercise 4: Your first orchestrated IoT Process](Exercise4/index.md)
  - Create the Model
  - Create the Forms
  - Deploy the Model
  - Start the `script-task` worker
  - Take picture
  - Handle the Process
    - Evaluate the Picture
    - Rate the costume
    - Estimate the age
    - Dispense Candy!
- [Exercise 5: Using Decision Tables](Exercise5/index.md)
  - Create Model
  - Create DMN Decision Table
  - Create Task worker
  - Start Task worker
  - Deploy DMN
  - Deploy Model
  - Take picture
  - Evaluate picture, rate the Costume and estimate the age
    - Dispense Candy!
- [Exercise 6: Using Google AI to automate decisions](Exercise6/index.md)
  - Third model
    - Create model
    - Create DMN
  - Create Task Worker
    - Start Task Worker
  - Take picture
    - Dispense Candy!
-