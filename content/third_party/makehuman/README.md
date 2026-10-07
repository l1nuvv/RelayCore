# MakeHuman anatomy source

CC0 assets from MakeHuman Community MPFB2, pinned to commit `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`.

- [base.obj](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/3dobjs/base.obj)
- [male.target.gz](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/macrodetails/caucasian-male-young.target.gz)
- License: `LICENSE.ASSETS.md`; the base mesh also carries its attribution and CC0 declaration in its header.

`scripts/author-scientist.py` applies the male shape and converts units. The head and fitted gloves use this anatomy; the loose cotton coat, trousers, safety boots, goggles, half-face respirator and rubber apron are authored separately. Hand placement follows the forearm joints. The cotton albedo is generated locally. Export through `scripts/export-native-lab-glb.py -- --model=scientist_hazmat` in Blender.
