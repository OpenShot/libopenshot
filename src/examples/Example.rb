# Find and load the ruby libopenshot wrapper library
require "./openshot"

# Create a new FFmpegReader and Open it
r = OpenShot::FFmpegReader.new("myfile.mp4")
r.Open()

# Get frame 1
f = r.GetFrame(1)

# Display the frame
r.Display()

