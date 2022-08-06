import json
import sys
from math import ceil, floor

import cv2
import numpy as np
import reedsolo

success = set()


# Random code I borrowed from somewhere do not touch
def bin_byte(b):
    return int(b, 2).to_bytes((len(b) + 7) // 8, byteorder='big')


def byte_bin(b):
    bi = '{:0{l}b}'.format(int.from_bytes(b, byteorder='big'), l=len(b) * 8)
    return ''.join([bi[i:i + 8] for i in range(0, len(bi), 8)])


def alpha_int(n):
    ans = 2 * alpha_int(n - 1) if n > 0 else 1
    if ans > 255:
        ans ^= 285
    return ans


def int_alpha(n):
    if n == 0:
        return 0
    if n == 1:
        return 0
    ans = 0
    while n != 1:
        ans += 1
        if n % 2 == 0:
            n //= 2
        else:
            n = (n ^ 285) // 2
    return ans


def mul(a, b, alpha):
    ans = [0] * (len(a) + len(b) - 1)
    for i in range(len(a)):
        for j in range(len(b)):
            if alpha:
                ans[i + j] ^= alpha_int((a[i] + b[j]) % 255)
            else:
                ans[i + j] ^= alpha_int((int_alpha(a[i]) +
                                        int_alpha(b[j])) % 255)
    return [int_alpha(i) for i in ans] if alpha else ans


def quiet_zones(output):
    cv2.rectangle(output, (0, 0),
                  (output.shape[0] - 1, output.shape[1] - 1), 0, -1)
    cv2.rectangle(output, (4, 4),
                  (output.shape[0] - 5, output.shape[1] - 5), 2, -1)


def finders(output):
    s = output.shape[0]
    cv2.rectangle(output, (4, 4), (11, 11), 0, -1)
    cv2.rectangle(output, (4, 4), (10, 10), 1, 1)
    cv2.rectangle(output, (6, 6), (8, 8), 1, -1)
    cv2.rectangle(output, (s - 5, 4), (s - 12, 11), 0, -1)
    cv2.rectangle(output, (s - 5, 4), (s - 11, 10), 1, 1)
    cv2.rectangle(output, (s - 7, 6), (s - 9, 8), 1, -1)
    cv2.rectangle(output, (4, s - 5), (11, s - 12), 0, -1)
    cv2.rectangle(output, (4, s - 5), (10, s - 11), 1, 1)
    cv2.rectangle(output, (6, s - 7), (8, s - 9), 1, -1)


def alignment_pattern(output):
    with open('alignment.json') as f:
        numbers = json.load(f)[(output.shape[0] - 29) // 4 - 1]
    for i, x in enumerate(numbers):
        for j, y in enumerate(numbers):
            if (i == 0 and j == 0) or (i == 0 and j == len(numbers) - 1) or (i == len(numbers) - 1 and j == 0):
                continue
            cv2.rectangle(output, (x - 2, y - 2), (x + 2, y + 2), 1, -1)
            cv2.rectangle(output, (x - 1, y - 1), (x + 1, y + 1), 0, 1)


def timing(output):
    for i in range(12, output.shape[0] - 12):
        output[i][10] = i % 2 ^ 1
        output[10][i] = i % 2 ^ 1


def encode_data(content, version, ec):
    with open('ecc.json') as f:
        specs = json.load(f)[version - 1][ec]
    goal = (specs[1] * specs[2] + specs[3] * specs[4]) * 8
    count = bin(len(content))[2:]
    content = byte_bin(content.encode('utf-8'))
    if 1 <= version <= 9:
        count = count.zfill(8)
    else:
        count = count.zfill(16)
    content = '0100' + count + content
    if len(content) < goal:
        content += '0000'
    for i in range((goal - len(content)) // 8):
        content += '11101100' if i % 2 == 0 else '00010001'
    return content


def _generator(n):
    if n == 1:
        return [1, 1]
    return mul([alpha_int(n - 1), 1], _generator(n - 1), False)


def reed_solomon(content, n):
    message = [int(content[i * 8:(i + 1) * 8], 2)
               for i in range(len(content) // 8)]
    message = [0] * n + message[::-1]
    generator = [0] * (len(message) - n - 1) + _generator(n)
    temp = []
    for i in range(len(message) - n):
        temp = mul((message[-1], ), generator, False)
        message = [message[i] ^ temp[i] for i in range(len(message) - 1)]
        generator.pop(0)
    message = message[::-1]
    return ''.join([bin(i)[2:].zfill(8) for i in message])


def ecc(content, version, ec):
    with open('ecc.json') as f:
        specs = json.load(f)[version - 1][ec]
    blocks = []
    for i in range(specs[1]):
        blocks.append(content[:specs[2] * 8])
        content = content[specs[2] * 8:]
    for i in range(specs[3]):
        blocks.append(content[:specs[4] * 8])
        content = content[specs[4] * 8:]
    # blocks = [block + reed_solomon(block, specs[0]) for block in blocks]
    blocks = [byte_bin(reedsolo.RSCodec(specs[0]).encode(
        bin_byte(block))) for block in blocks]
    content = ''
    if specs[3] == 0:
        for _ in range(specs[2]):
            for i in range(specs[1]):
                content += blocks[i][:8]
                blocks[i] = blocks[i][8:]
    else:
        for _ in range(specs[2]):
            for i in range(len(blocks)):
                content += blocks[i][:8]
                blocks[i] = blocks[i][8:]
        for _ in range(specs[4] - specs[2]):
            for i in range(specs[1], specs[1] + specs[3]):
                content += blocks[i][:8]
                blocks[i] = blocks[i][8:]
    for _ in range(specs[0]):
        for i in range(len(blocks)):
            content += blocks[i][:8]
            blocks[i] = blocks[i][8:]
    return content


def remainder(content, version):
    with open('remainder.json') as f:
        remainder = json.load(f)[version - 1]
    return content + '0' * remainder


def format_string(output, ec, mask):
    format = bin(('M', 'L', 'H', 'Q').index(ec) << 3 | mask)[2:]
    generator = '10100110111'
    ecc = format + '0' * 10
    while len(ecc) > 10:
        padded_generator = generator + '0' * (len(ecc) - len(generator))
        ecc = bin(int(ecc, 2) ^ int(padded_generator, 2))[2:]
    format += ecc.zfill(10)
    format = bin(int(format, 2) ^ 21522)[2:].zfill(15)
    for i in range(7):
        output[12][-5 - i] = int(format[i])
        output[4 + floor(1.17 * i)][12] = int(format[i])
    for i in range(8):
        output[-5 - i][12] = int(format[-i - 1])
        output[12][4 + floor(1.17 * i)] = int(format[-i - 1])


def version_string(output, version):
    version = bin(version)[2:]
    generator = '1111100100101'
    ecc = version + '0' * 12
    while len(ecc) > 12:
        padded_generator = generator + '0' * (len(ecc) - len(generator))
        ecc = bin(int(ecc, 2) ^ int(padded_generator, 2))[2:]
    version = version.zfill(6)
    version += ecc.zfill(12)
    for i in range(6):
        for j in range(3):
            output[4 + i][-15 + j] = version[i * 3 + j]
            output[-15 + j][4 + i] = version[i * 3 + j]


def place(output, content):
    i = output.shape[1] - 1
    while i > 0:
        if i == 10:
            i -= 1
            continue
        if i % 4 == 0 or i % 4 == 3:
            for j in range(output.shape[0] - 1, -1, -1):
                if output[i][j] == 2:
                    success.add((i, j))
                    if not content:
                        output[i][j] = 3
                    else:
                        output[i][j] = content[0]
                        content = content[1:]
                i -= 1
                if output[i][j] == 2:
                    success.add((i, j))
                    if not content:
                        output[i][j] = 3
                    else:
                        output[i][j] = content[0]
                        content = content[1:]
                i += 1
        else:
            for j in range(output.shape[0]):
                if output[i][j] == 2:
                    success.add((i, j))
                    if not content:
                        output[i][j] = 3
                    else:
                        output[i][j] = content[0]
                        content = content[1:]
                i -= 1
                if output[i][j] == 2:
                    success.add((i, j))
                    if not content:
                        output[i][j] = 3
                    else:
                        output[i][j] = content[0]
                        content = content[1:]
                i += 1
        i -= 2


def _mask(mask, y, x):
    # return 0
    if (x, y) not in success:
        return 0
    x -= 4
    y -= 4
    if mask == 0:
        return int((x + y) % 2 == 0)
    if mask == 1:
        return int(x % 2 == 0)
    if mask == 2:
        return int(y % 3 == 0)
    if mask == 3:
        return int((x + y) % 3 == 0)
    if mask == 4:
        return int((x // 2 + y // 3) % 2 == 0)
    if mask == 5:
        return int((x * y) % 2 + (x * y) % 3 == 0)
    if mask == 6:
        return int(((x * y) % 3 + x * y) % 2 == 0)
    if mask == 7:
        return int(((x * y) % 3 + x + y) % 2 == 0)


def mask(output, mask):
    for i in range(output.shape[0]):
        for j in range(output.shape[1]):
            output[i][j] ^= _mask(mask, i, j)


def _evaluate1(output):
    penalty = 0
    for i in range(4, output.shape[1] - 4):
        j = 4
        k = 4
        while k < output.shape[0] - 4:
            if output[i][j] == output[i][k]:
                if k - j == 4:
                    penalty += 3
                elif k - j > 4:
                    penalty += 1
            else:
                j = k
            k += 1
    for i in range(4, output.shape[0] - 4):
        j = 4
        k = 4
        while k < output.shape[1] - 4:
            if output[j][i] == output[k][i]:
                if k - j == 4:
                    penalty += 3
                elif k - j > 4:
                    penalty += 1
            else:
                j = k
            k += 1
    return penalty


def _evaluate2(output):
    kernel = np.array(((1, 1), (1, 1)))
    temp = cv2.filter2D(output[4:-4, 4:-4], -1, kernel, anchor=(0, 0))
    return sum([sum([int(j % 4 == 0) for j in i]) for i in temp]) * 3


def _evaluate3(output):
    penalty = 0
    kernel = np.array(((1, -1, 1, 1, 1, -1, 1, -1, -1, -1, -1),))
    temp = cv2.filter2D(output[4:-4, 4:-4], -1, kernel, anchor=(0, 0))
    penalty += sum([sum(i) for i in temp[:, :-4] // 5])
    kernel = kernel[::-1]
    temp = cv2.filter2D(output[4:-4, 4:-4], -1, kernel, anchor=(0, 0))
    penalty += sum([sum(i) for i in temp[:, :-4] // 5])
    kernel = kernel.reshape(kernel.shape[::-1])
    temp = cv2.filter2D(output[4:-4, 4:-4], -1, kernel, anchor=(0, 0))
    penalty += sum([sum(i) for i in temp[:-4, :] // 5])
    kernel = kernel[::-1]
    temp = cv2.filter2D(output[4:-4, 4:-4], -1, kernel, anchor=(0, 0))
    penalty += sum([sum(i) for i in temp[:-4, :] // 5])
    return penalty * 40


def _evaluate4(output):
    a = sum(sum(output)) / output.size * 100
    b = abs(ceil(a / 5))
    a = abs(floor(a / 5))
    a, b = abs(a - 50), abs(b - 50)
    a, b = a // 5, b // 5
    return min(a, b) * 10


def evaluate(output):
    penalty = 0
    penalty += _evaluate1(output)
    penalty += _evaluate2(output)
    penalty += _evaluate3(output)
    penalty += _evaluate4(output)
    return penalty


def test_mask(output, ec):
    penalties = []
    for i in range(8):
        format_string(output, ec, i)
        mask(output, i)
        penalties.append(evaluate(output))
        mask(output, i)
    mask(output, penalties.index(min(penalties)))
    return penalties.index(min(penalties))


def generate_qr(content, version, ec):
    version = int(version)
    output = np.empty(
        ((version - 1) * 4 + 29, (version - 1) * 4 + 29), dtype=np.uint8)
    for i in range(output.shape[0]):
        for j in range(output.shape[1]):
            output[i][j] = 2
            # output[i][j] = (i + j) % 2
    content = encode_data(content, version, ec)
    content = ecc(content, version, ec)
    content = remainder(content, version)
    quiet_zones(output)
    finders(output)
    if version != 1:
        alignment_pattern(output)
    timing(output)
    # Dark module
    output[12][-12] = 1
    format_string(output, 'M', 0)
    if version >= 7:
        version_string(output, 0)
    # place(output, '0000' * 584)
    place(output, content)
    if version >= 7:
        version_string(output, version)
    format_string(output, ec, test_mask(output, ec))
    return ''.join([''.join([str(int(j)) for j in i]) for i in output])


def main():
    args = json.load(sys.stdin)
    print(generate_qr(args['content'], args['version'], args['ec']), end='')


if __name__ == '__main__':
    main()
