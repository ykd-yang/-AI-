from flask import Flask, jsonify, request, make_response

app = Flask(__name__)
app.json.ensure_ascii = False  # Ensure UTF-8 JSON responses

USER_DATA = [
    {
        "name": "홍길동",
        "age": 30,
        "height": 175,
        "item": ["sword", "shield", "potion"],
    },
    {
        "name": "임꺽정",
        "age": 32,
        "height": 180,
        "item": ["bow", "arrow", "cloak"],
    },
    {
        "name": "이몽룡",
        "age": 28,
        "height": 170,
        "item": ["fan", "ring", "letter"],
    },
    {
        "name": "성춘향",
        "age": 27,
        "height": 165,
        "item": ["hairpin", "bracelet", "perfume"],
    },
]

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="utf-8" />
    <title>POST 테스트</title>
    <style>
        @import url('https://cdn.jsdelivr.net/gh/orioncactus/pretendard/dist/web/static/pretendard.css');
        :root {
            color-scheme: light;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 48px 24px;
            background: radial-gradient(120% 120% at 85% 10%, #00d0ff 0%, #0066ff 45%, #001b44 100%), #0f172a;
            font-family: "Pretendard", "Noto Sans KR", system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            color: #0f172a;
        }
        .container {
            width: min(520px, 100%);
            background: rgba(255, 255, 255, 0.9);
            border-radius: 28px;
            padding: 40px 44px;
            display: flex;
            flex-direction: column;
            gap: 32px;
            box-shadow: 0 32px 60px rgba(15, 23, 42, 0.18);
            border: 1px solid rgba(148, 163, 184, 0.16);
            backdrop-filter: blur(18px);
        }
        .eyebrow {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            padding: 6px 16px;
            border-radius: 999px;
            background: rgba(0, 102, 255, 0.12);
            color: #1d4ed8;
            font-weight: 600;
            font-size: 14px;
        }
        header h1 {
            font-size: 32px;
            line-height: 1.25;
            letter-spacing: -0.03em;
            margin-bottom: 12px;
        }
        header p {
            color: #475569;
            font-size: 16px;
            line-height: 1.6;
        }
        form {
            display: flex;
            align-items: center;
            gap: 12px;
        }
        label {
            position: absolute;
            width: 1px;
            height: 1px;
            padding: 0;
            margin: -1px;
            overflow: hidden;
            clip: rect(0, 0, 0, 0);
            white-space: nowrap;
            border: 0;
        }
        input {
            flex: 1;
            padding: 16px 18px;
            border-radius: 16px;
            border: 1px solid rgba(148, 163, 184, 0.35);
            background: #f8fafc;
            font-size: 16px;
            transition: border-color 0.2s, box-shadow 0.2s, background 0.2s;
        }
        input:focus {
            outline: none;
            border-color: #2563eb;
            box-shadow: 0 0 0 4px rgba(37, 99, 235, 0.2);
            background: #fff;
        }
        button {
            padding: 16px 24px;
            border-radius: 16px;
            border: none;
            font-size: 16px;
            font-weight: 600;
            color: #fff;
            background: linear-gradient(135deg, #0066ff 0%, #00d0ff 100%);
            cursor: pointer;
            transition: transform 0.2s ease, box-shadow 0.2s ease;
        }
        button:hover {
            transform: translateY(-1px);
            box-shadow: 0 18px 32px rgba(0, 102, 255, 0.35);
        }
        button:active {
            transform: translateY(0);
            box-shadow: 0 10px 22px rgba(0, 102, 255, 0.28);
        }
        .result-block {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }
        .result-block h2 {
            font-size: 18px;
            font-weight: 600;
            color: #0f172a;
        }
        pre {
            margin: 0;
            background: #0f172a;
            color: #e2e8f0;
            padding: 20px;
            border-radius: 18px;
            min-height: 160px;
            font-size: 14px;
            line-height: 1.6;
            overflow-x: auto;
            white-space: pre-wrap;
            word-break: break-word;
        }
        .name-hints {
            padding: 20px;
            border-radius: 18px;
            background: rgba(15, 23, 42, 0.04);
            border: 1px solid rgba(148, 163, 184, 0.18);
            display: flex;
            flex-direction: column;
            gap: 12px;
        }
        .name-hints h3 {
            font-size: 16px;
            font-weight: 600;
            color: #1e3a8a;
        }
        .name-hints ul {
            list-style: none;
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
        }
        .name-hints li {
            padding: 8px 12px;
            border-radius: 999px;
            background: rgba(59, 130, 246, 0.12);
            color: #1d4ed8;
            font-size: 14px;
            font-weight: 500;
        }
        @media (max-width: 600px) {
            body {
                padding: 32px 16px;
            }
            .container {
                padding: 32px 28px;
                gap: 28px;
            }
            form {
                flex-direction: column;
                align-items: stretch;
            }
            button {
                width: 100%;
            }
        }
    </style>
</head>
<body>
    <main class="container">
        <span class="eyebrow">API 테스트</span>
        <header>
            <h1>사용자 정보 요청</h1>
            <p>이름을 입력하면 실시간으로 사용자 데이터를 조회할 수 있어요.</p>
        </header>
        <form id="user-form">
            <label for="name-input">이름</label>
            <input id="name-input" name="name" placeholder="예: 홍길동" required />
            <button type="submit">조회하기</button>
        </form>
        <section class="result-block">
            <h2>응답 결과</h2>
            <pre id="result">POST 요청의 응답이 여기에 표시됩니다.</pre>
        </section>
        <section class="name-hints">
            <h3>요청 가능한 이름</h3>
            <ul>
                <!--NAME_LIST-->
            </ul>
        </section>
    </main>
    <script>
        const form = document.getElementById('user-form');
        const nameInput = document.getElementById('name-input');
        const result = document.getElementById('result');

        form.addEventListener('submit', async (event) => {
            event.preventDefault();
            const nameValue = nameInput.value.trim();
            if (!nameValue) {
                result.textContent = '이름을 입력해 주세요.';
                return;
            }
            try {
                const response = await fetch('/user', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json;charset=UTF-8',
                        'Accept': 'application/json',
                    },
                    body: JSON.stringify({ name: nameValue }),
                });

                const text = await response.text();
                result.textContent = text;
            } catch (error) {
                result.textContent = '요청 중 오류가 발생했어요: ' + error;
            }
        });
    </script>
</body>
</html>
"""



def _render_name_list() -> str:
    return "\n".join(
        f"                <li>{user['name']}</li>" for user in USER_DATA
    )


def _build_html_form() -> str:
    names_markup = _render_name_list()
    return HTML_TEMPLATE.replace("<!--NAME_LIST-->", names_markup)

def _filter_users_by_name(name: str):
    if not name:
        return USER_DATA
    return [user for user in USER_DATA if user["name"] == name]


@app.route("/user", methods=["GET", "POST"])
def handle_user():
    """Return users optionally filtered by name for both GET and POST."""
    name = None
    if request.method == "GET":
        name = request.args.get("name", default="")
    else:
        if request.is_json:
            payload = request.get_json(silent=True) or {}
            name = payload.get("name", "")
        else:
            name = request.form.get("name", "")
    filtered = _filter_users_by_name(name.strip())
    if len(filtered) == 1:
        response = jsonify(filtered[0])
    else:
        response = jsonify(filtered)
    response.headers["Content-Type"] = "application/json; charset=utf-8"
    return response


@app.route("/tester", methods=["GET"])
def tester():
    """Simple HTML page to test POST requests from the browser."""
    response = make_response(_build_html_form())
    response.headers["Content-Type"] = "text/html; charset=utf-8"
    return response


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=4000)
