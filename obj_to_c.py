import sys

def obj_to_c_array(filename, output_filename="obj.h"):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Ошибка: файл {filename} не найден.")
        return

    with open(output_filename, 'w') as out:
        out.write("// Сгенерировано автоматически из {}\n".format(filename))
        out.write("const char* obj[] = {\n")
        count = 0
        for line in lines:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('v ') or line.startswith('f '):
                escaped_line = line.replace('"', '\\"')
                out.write(f'    "{escaped_line}",\n')
                count += 1
        out.write("};\n")
        out.write(f"// Всего строк: {count}\n")

    print(f"Файл {output_filename} создан. Скопируй его в папку с проектом.")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Использование: python obj_to_c.py model.obj")
    else:
        obj_to_c_array(sys.argv[1])