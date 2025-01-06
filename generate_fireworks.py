import png
import random
import math

frames = 150 # Number of frames to generate
interval = 10 # Number of frames between fireworks
colors = [
    [255, 0, 0], # Red
    [0, 255, 0], # Green
    [0, 0, 255], # Blue
    [191, 64, 191], # Purple
    [255, 172, 28], # Orange
    [255, 255, 255], # White
    [253, 218, 13] # Yellow
] # Possible firework colors
decayTime = 15 # Duration of time before particles decay
decayLength = 40 # How long it takes for a particle to decay
particleCount = 20 # Number of particles of a size 1 firework
friction = 0.90 # Air resistance for particles
seed = 2 # Random seed

# A firework is a tuple (position, size, color, index)
fireworks = []

random.seed(seed)
for i in range(0, frames // interval):
    position = [random.uniform(4, 27), random.uniform(2, 13)]
    size = random.uniform(0.5, 1)
    color = colors[random.randint(0, len(colors) - 1)]
    index = i
    fireworks.append([position, size, color, index])

# A particle is a tuple (position, velocity, baseColor, age)
particles = []
pngWriter = png.Writer(width=32, height=16, bitdepth=8, greyscale=False)
for i in range(0, frames * 2):
    # Update the simulation
    if i % interval == 0:
        # Create the particles for a firework
        position, size, color, index = fireworks[(i // interval) % (frames // interval)]
        random.seed(seed + index)
        numParticles = math.floor(particleCount * size)
        for j in range(0, numParticles):
            particlePosition = position.copy()
            angle = random.uniform(0, 2 * math.pi)
            speed = random.uniform(0.3, 1.0) * size
            particleVelocity = [math.cos(angle) * speed, math.sin(angle) * speed]
            particleBaseColor = color
            particleAge = 0
            particles.append([particlePosition, particleVelocity, particleBaseColor, particleAge])

    for p in particles:
        p[0][0] += p[1][0] # Update position from velocity
        p[0][1] += p[1][1] # Update position from velocity
        p[1][0] *= friction # Update velocity
        p[1][1] *= friction # Update velocity
        p[3] += 1 # Increase the age

    # Write the PNG
    rows = []
    for y in range(0, 16):
        row = []
        for x in range(0, 32 * 3):
            row.append(0)
        rows.append(row)
    for p in particles:
        position = p[0]
        color = p[2]
        age = p[3]
        if position[0] < 0 or position[0] >= 32 or position[1] < 0 or position[1] >= 16:
            continue
        if age > decayTime + decayLength:
            continue
        adjustedColor = color.copy()
        if age > decayTime:
            adjustedColor[0] = int(color[0] * ((decayTime + decayLength - age) / decayLength))
            adjustedColor[1] = int(color[1] * ((decayTime + decayLength - age) / decayLength))
            adjustedColor[2] = int(color[2] * ((decayTime + decayLength - age) / decayLength))
        rows[math.floor(position[1])][math.floor(position[0]) * 3] = adjustedColor[0]
        rows[math.floor(position[1])][(math.floor(position[0]) * 3) + 1] = adjustedColor[1]
        rows[math.floor(position[1])][(math.floor(position[0]) * 3) + 2] = adjustedColor[2]

    with open("fireworks/" + str(i) + ".png", "wb") as f:
        pngWriter.write(f, rows)
