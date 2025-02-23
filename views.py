from app import app, db
from .forms import SignupForm, LoginForm
from .models import User
from flask_login import current_user, login_user, logout_user, login_required
from flask import render_template, flash, redirect, url_for, request

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/login', methods=['GET', 'POST'])
def login():
    form = LoginForm()
    if form.validate_on_submit():
        user = User.query.filter_by(email=form.email.data).first()
        
        # ✅ 如果用户不存在 或 密码错误
        if not user or not check_password_hash(user.password, form.password.data):
            flash('登录失败，请检查电子邮件和密码', 'danger')
            return render_template('login.html', title='登录', form=form)
        
        # ✅ 登录用户
        login_user(user)
        flash('登录成功！', 'success')
        return redirect(url_for('dashboard'))
    
    return render_template('login.html', title='登录', form=form)

@app.route('/register', methods=['GET', 'POST']) 
def register():
    form = SignupForm()
    if form.validate_on_submit():
        if User.query.filter_by(email=form.email.data).first():
            flash('电子邮件已注册，请使用其他邮箱', 'danger')
            return render_template('register.html', title='注册', form=form)
        
        new_user = User(
            full_name=form.full_name.data,
            email=form.email.data,
            phone=form.phone.data,
            password=form.password.data,
            payment_method=form.payment_method.data
        )
        db.session.add(new_user)
        db.session.commit()
        login_user(new_user)
        flash('账户创建成功！', 'success')
        return redirect(url_for('dashboard'))
    return render_template('register.html', title='注册', form=form)

@app.route('/dashboard')
@login_required
def dashboard():
    return render_template('dashboard.html', title="用户中心")

@app.route('/logout')
@login_required
def logout():
    logout_user()
    flash('您已退出登录', 'info')
    return redirect(url_for('login'))
