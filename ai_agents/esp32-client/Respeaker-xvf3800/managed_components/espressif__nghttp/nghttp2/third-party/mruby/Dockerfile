FROM ruby:3.2.2-bullseye

RUN apt-get update && apt-get install --no-install-recommends -y python3-pip shellcheck \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY Gemfile Gemfile.lock .pre-commit-config.yaml ./

RUN bundle install && pip3 install pre-commit && git init . && pre-commit install-hooks

COPY . .
