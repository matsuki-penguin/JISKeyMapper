python -c "from PIL import Image; Image.open('on_us_32.png').save(r'..\res\on_us.ico', format='ICO', append_images=[Image.open(f) for f in ['on_us_24.png', 'on_us_20.png', 'on_us_16.png']], bitmap_format=False)"
python -c "from PIL import Image; Image.open('off_us_32.png').save(r'..\res\off_us.ico', format='ICO', append_images=[Image.open(f) for f in ['off_us_24.png', 'off_us_20.png', 'off_us_16.png']], bitmap_format=False)"
python -c "from PIL import Image; Image.open('on_jp_32.png').save(r'..\res\on_jp.ico', format='ICO', append_images=[Image.open(f) for f in ['on_jp_24.png', 'on_jp_20.png', 'on_jp_16.png']], bitmap_format=False)"
python -c "from PIL import Image; images = [Image.open(f) for f in ['off_jp_24.png', 'off_jp_20.png', 'off_jp_16.png']]; Image.open('off_jp_32.png').save(r'..\res\off_jp.ico', format='ICO', append_images=images, bitmap_format=False)"
python -c "from PIL import Image; img = Image.open('app_256.png'); img.save('../res/app.ico', format='ICO', append_images=[Image.open('app_48.png'), Image.open('app_32.png'), Image.open('app_16.png')], bitmap_format=False)"
pause
