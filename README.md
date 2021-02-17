# Learning Multiple-Scattering Solutions for Sphere-Tracing of Volumetric Subsurface Effects (Eurographics 2021)

L. Leonard, K. Höhlein, R. Westermann



This step-by-step guide is intended for those who wants to reproduce the technique presented in the paper. The main idea is to demonstrate a speed-up can be obtained learning the scattering process occurring inside a specific geometry (sphere) and then using the conditional variational autoencoder (CVAE) in a shader to accelerate a traditional volumetric path-tracer. We also provide the solution using a traditional tabular approach for the distribution, with the settings exposed in Mueller, et al, 2016 and an expansion used to generate all required variables.

The proper order to use the CVAE solution is:

- Generate the data
- Train the model
- Compile the model to HLSL code
- Render a scene using DX12 with the CVAE shader

## Generating the data

The project to generate the scattering events in a sphere and save the statistics is provided in folder `subsurface_cvae_generating`. In the `SphereScattering` `Program.cs` code you may uncomment the desired method to generate the specific data. The generation process might take depending on the machine between 12 hours to a day.  

```c#
static void Main(string[] args)
{
    //// Used to generate 4 million samples for CVAE training
    //GenerateDatasetForTrainingCVAE();

    //// Used to Generate Mueller data tables
    //GenerateDatasetForTabulationMethod();

    // Used to generate samples for extended table
    // Generates 8 files of 16 million samples each...
    GenerateDatasetForTabulationXMethod();
}
```

The data for the CVAE training is a .csv file named `ScattersDataSet.ds`. The other two tables are already `.bin` files, and they must be copied to the output folder of the rendering project.

## Training the CVAE

The training project (folder `subsurface_cvae_training`) uses `torch` and  `numpy` modules. 

The first step is to convert the `.ds` file generated with the scattering process as a `.npz` file.  Copy the generated `ScattersDataSet.ds` file to a created `DataSets` folder in the project folder. Then run the `main_convertDS.py` python module. Notice an equivalent file `ScattersDataSet.npz` was created.

To train the three CVAE models (len, path and scat) it is necessary to run the modules: `main_train_lenGen.py`, `main_train_pathGen.py` and `main_train_scatGen.py` respectively.

We can specify in the script different settings to train.

```python
lenGenConfigs = [
    {
        'depth': 3,
        'width': 8,
        'activation': nn.Softplus,
        'latent': 2
    }
]
```

An id is generated for each training settings and saved in a Running folder.

It is possible to examine the learnt distributions using the scripts `main_test_lenGen.py`...

## Compiling the models to HLSL

In order to compile the desired models into HLSL can be used the script `main_compiling.py`. Notice in the code a single configuration will be finally compiled for each model type. The final compilation is a single file code saved as `compiledModels.h` where each model is translated to an HLSL function that receives the values for the decoder in an array, and returns in an out array the values of mu and sigma for the variables (first all mu and then all sigma).

```c++
void lenModel(float _input[4], out float _output[2]) { ... }
```

This code is HLSL and thus can be included in any shader.

## Rendering subsurface effects with DX12

The rendering project is in `subsurface_cvae_rendering` folder. The implementation in RTX of the CVAE technique and other methods can be found in the project `dx4xb.Techniques`. This project requires a PC with RTX technology support.

The models are looked at the desktop folder in a Models folder. All models used to tests are provided here.

The settings used in the paper can be executed via `Eurographics.CVAE_ST` project. In that project, a series of include statements can be commented and uncommented to define the desired settings.

```c++
#include "Teasser.h"
//#include "Comparisons.h"
//#include "ComparisonsLucy.h"
//#include "ComplexityBunny.h"
//#include "ComplexityLucy.h"
//#include "Absorptions.h"
//#include "NEETests.h"
```

 Inside each of those files there is a scene setup and a technique (or set of techniques used). Also, there is some `define` values can be used to toggle among different settings.

