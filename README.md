Generative audiovisual sculpture based on the Mandelbulb fractal.

Built for the avr (abstract virtual reality) framework and Vive Pro headset.

Dependencies:

OpenGL
Glew
GLFW
CSound
RapidLib
OpenVR

The main loop is run from AvrApp. This calls VR::HandleInput() 
followed by Graphics::RenderFrame() which can be found in Visuals/.

All of the calls to OpenVR can be found in the VR directory.

Graphics::Renderframe() calls the Fivecell class. This is where all the 
audio and graphics are generated.

Fivecell is an old name that I need to change as it doesn't describe
the function of the class. This class is set up like an openFrameworks
OfApp class with setup(), update() and draw() functions.

CSound is run on a separate thread created in Fivecell::setup().

The main frag shader is called mengerGlass.frag and the relevant
vertex shader is mengerGlass.vert. All the graphics are generated
with these. Again, the names of the files are old and need to be 
updated as this piece is based on a Mandelbulb fractal rather
than the Menger Cube.

If not connected to a Vive Pro headset the application can be 
run in dev mode by including the -dev flag when running the 
avr binary.

This piece will also incorporate audio and visual mapping using a neural
network that is provided by RapidLib.
