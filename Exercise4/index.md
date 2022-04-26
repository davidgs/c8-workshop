# Exercise 4: Your first orchestrated IoT Process

## Building the Models

You don't actually _have_ to build the models if you don't want to as they are all provided for you here:

- [Exercise 4 Model](./exercise4.bpmn)
- [Evaluate Costume Form](./evaluate-costume.form)
- [Rate Costume Form](./rate-costume.form)
- [Estimate Age Form](./estimate-age.form)

You can drag and drop these 4 files into your Camunda Web Modeler to see the models. I've created folders for each exercise, but you don't have to.

[Exercise 4 Folder](./images/exercise4-folder.png)

If you open the `exercise4.bpmn` you will see that all the forms are already linked, and that the `Task Workers` are all filled out as well. We will be re-using the `ScriptTaskWorker` from [Exercise 2](../Exercise2/index.md) to handle all the 'candy' in this model.

You can now Deploy this model, but don't start a process instance just yet. The IoT Hardware we built in [Exercise 3](../Exercise3/index.md) will be starting all the instances of this process for us.

## Testing the Process

From within the Web Modeler you can always start an instance of the process to test it. You will need to click the `Start Process` button on the top right of the modeler and add a JSON object containing some information to start the process:

```json
{"imageLoc": "https://davidgs.com:5050/pix/headshot.png", "isPicture": true }
```

That picture exists, and will be used to start an instance of the process to be evaluated.

## Starting a Process Instance

Now that we have deployed the model, we can use the Camera to start a process. It's as simple as pressing the second button on your camera board! You should see the bright Flash LED come on and then the camera will take a picture.

This picture will then be sent to a server process (that is running on my cloud server) and that server process will start your process instance with the picture you took.

**Note:** At the present time there is no mechanism for displaying the picture in-line in your form. This is a current shortcoming of the Camunda Platform 8 Forms implementation but I have submitted a feature request to the Camunda Platform team to have this added. For now, your form will have an uneditable field that contains the complete URL to your picture. You can copy/paste this URL into a new browser tab to see the picture.

If you go to the `Operate` Tab in C8, you should now see a running instance of your process:

[Exercise 4 Process Instance](./images/exercise4-process-instance.png)

## Completing Tasks

Now that the process has been started by your picture, you can go to the Task List tab in Camunda Platform 8 where you should now see a new Task:

[Exercise 4 Task List](./images/exercise4-task-list.png)

You will need to click the `claim` button in order to be able to make a selection on if this is a picture of a person in a costume or not.

Once you click the `Complete Task` button the task will disappear from your task list. If you go back to your `Operate` Tab you can see the token moving along the diagram. The path that it has taken so far will be highlighted in blue.

[Exercise 4 Task List](./images/exercise4-task-list.png)

You will now need to go back and forth between the `Operate` Tab and the `Task List` Tab to see the process move along the diagram as you complete the tasks in the Task List.

At the end, you should hear Skittles come out of the dispenser at the front of the room.

> **Note:** You can also build and start the go process contained in this directory by running the following command in your terminal:
>
> ```shell
> $ go build
> $ ./dispense-candy
> ```
>
> Which will simply build and run the go process. This Go process will print out the number of candy pieces to dispense rather than sending them to the actual Candy Dispenser.

## Lessons Learned

As you have noticed, this is a very user-intensive task. You have to repeatedly claim a task and answer a question about the picture. This is a very time consuming process. In the next exercise we will see how we can streamline this process by using DMN decision tables.

[Next: Exercise 5: Using Decision Tables](../Exercise5/index.md)