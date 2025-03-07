# workshop_stressometer
Using and creating arduino Nano 33 BLE sense rev2 to train own ai-recognetion model

We worked in a team of two between 42 student and art school student.

The goal of this workshop was : AI - Arduino - Psychological issues

Overall, the goal was to reflect on what is the norme, what is the norm in an AI.

Our approach is based in the childhood, where most of the trauma begin. 
In order to ilustrate the trauma using arduino and an AI, we decided to base on reasoning on "traumatizing" a child.

So, we trained an AI using Edge-Impulse to recognize some predetermined sentences. The library is in the github (the sentences are in french).

We portrayed the trauma using a stress variable. This variable is influenced by different factors :

    - The mouvment of the arduino
    - The proximity of someting near the arduino
    - The level of sound the arduino hears
    - The recognetion of the sentences
    
In order to show the trauma, the stress variable move up and down and influence a base-stress variable that start at 0. Once the base-stress variable moves up, the only way to lower it is to appease the "child".

How we trained the ai-model :

We make lot of record on Edge Impusle with different label like "Stress" and "calm" in data acquisition. After that we create an Impulse design with MFCC block audioand and add a classifier, save your impusle.
Now you can test or train every category.If you are fine go to deployment and choose your output as "arduino or C++"


How to proceed :
    
    - First, install the ai-trained-library in the arduino IDE
    - Second, you need to push the "retrieve_data.ino" in you arduino board (The board we used is the Arduino Nano 33 BLE sense REV2)
    - Then, in VSC (or other) run the code

In the end, the project works, but is open to a lot of improvment.
For exemple :
    - The proximity captor dont have any influence on the stress
        - The proximity should lower the base-stress, but it doesnt...
    - The ai-recognition isn't always spot-on, leading to a variationof the stress when nothing is happening, this comes from the ai-training in edge-impulse directly
    - There is a slight delay in reaction (but nothing too big)
    - The sound level doesnt work because the mic is already used by the ai-recognition model
