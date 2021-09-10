import json
import textwrap

import requests


USER_NAME = 'dilmnqvovpnmlib'
REPOSITORY_NAME = 'hakiwata'
API_URL = 'https://api.github.com/repos/{}/{}/contents/content/post/20210830/media'.format(
    USER_NAME,
    REPOSITORY_NAME,
)
REPOSITORY_BASE_URL = 'https://github.com/{}/{}/blob/main/'.format(
    USER_NAME,
    REPOSITORY_NAME,
)
GENERATE_FILE_NAME = 'README.md'


def get_latest_data():
    res = requests.get(API_URL)
    res_json = json.loads(res.text)
    latest_data = {
        'name': res_json[-1]['name'],
        'path': REPOSITORY_BASE_URL + res_json[-1]['path'],
    }
    return latest_data


def main():
    latest_data = get_latest_data()
    file_name = latest_data['name']
    file_path = latest_data['path']
    title = file_name.split('.')[0]
    base_content = """\
        # honOS
        
        - [ゼロからのOS自作入門](https://www.amazon.co.jp/%E3%82%BC%E3%83%AD%E3%81%8B%E3%82%89%E3%81%AEOS%E8%87%AA%E4%BD%9C%E5%85%A5%E9%96%80-%E5%86%85%E7%94%B0-%E5%85%AC%E5%A4%AA/dp/4839975868) を参考に自作 OS に取り組む。
        
        # 画面

        ## {}
        
        ![{}]({})
        
        ## 参考
        
        - [uchan-nos/mikanos](https://github.com/uchan-nos/mikanos)
    """.format(title, file_name, file_path)

    docs = textwrap.dedent(base_content)

    with open(GENERATE_FILE_NAME, 'w') as f:
        f.write(docs)


if __name__ == '__main__':
    main()
