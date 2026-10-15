"""Generate a synthetic Gource custom log for trying the galaxy layout without a repo.

    python tools/make_demo_log.py > demo.log
    gource.exe demo.log
"""
import random

random.seed(7)
tops = ['engine', 'render', 'audio', 'network', 'ui', 'tools', 'assets', 'gameplay', 'ai', 'physics']
mids = ['core', 'util', 'math', 'io', 'impl', 'tests', 'gl', 'vk', 'dx', 'net', 'sys']
users = ['ada', 'linus', 'grace', 'ken', 'dennis', 'margaret']
exts = ['cpp', 'h', 'lua', 'py', 'md', 'json']

t = 1600000000
for _ in range(1500):
    t += random.randint(600, 4000)
    path = random.choice(tops)
    for _ in range(random.randint(1, 4)):
        path += '/%s%d' % (random.choice(mids), random.randint(0, 2))
    path += '/file%d.%s' % (random.randint(0, 40), random.choice(exts))
    print('%d|%s|%s|%s' % (t, random.choice(users), 'A' if random.random() < 0.8 else 'M', path))
